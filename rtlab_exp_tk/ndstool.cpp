/***************************************************************************
                          ndstool.cpp  -  Daq System NDS Repair Tool
                             -------------------
    begin                : Mon Oct 28 2002
    copyright            : (C) 2002 by Calin Culianu
    email                : calin@rtlab.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#define _GNU_SOURCE 1
#include <iostream>
#include <map>
#include <vector>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <qstring.h>
#include <qfile.h>
#include <qtextstream.h>
#include "dsdstream.h"
#include "dsd_repair.h"
#include "common.h"
#include "exception.h"
#include "tempspooler.h"

using namespace std;

struct OpState 
{
  virtual ~OpState() {};
  virtual int  constraintsError() const = 0;
  virtual bool checkConstraints() = 0;
};

struct Op 
{
  typedef void (Op::*ArgCallback_t)(const QString &argValue);
  
  virtual ~Op() { delete op_state; }
  virtual int doIt() = 0;

  QString name;
  QString description;
  
  typedef pair<QString, ArgCallback_t> ArgsMapValue_t;
  typedef map<QString, ArgsMapValue_t> ArgsMap_t;

  /* A map of arg-name -> (arg-description, arg-action/callback) */
  ArgsMap_t  allArgs;  

  virtual OpState *state() { return op_state; }
 protected:
  OpState *op_state;
};

struct SplitOpState: public OpState
{
  SplitOpState() : infile(QString::null), outfile(QString::null), 
                   start(0), count(ULONG_LONG_MAX) {};

  int  constraintsError() const;
  bool checkConstraints();


  QString infile;
  QString outfile;
  scan_index_t start;
  scan_index_t count;
};

struct SynopsisOpState: public OpState 
{
  bool checkConstraints() { return true; }
  int  constraintsError() const;
};

struct InfoOpState: public OpState 
{
  bool checkConstraints();
  int  constraintsError() const;
  QString filename;
};

struct RepairOpState: public OpState 
{
  RepairOpState() : outfile("RECOVERED.nds") {};
  bool checkConstraints();
  int  constraintsError() const;
  QString filename;
  QString outfile;
};

struct SplitOp: public Op 
{
  SplitOp();

  int doIt();
  void buildAllArgs();
  void infileArg(const QString &infile);
  void outfileArg(const QString &outfile);
  void startArg(const QString &start);
  void countArg(const QString &count);
  SplitOpState *state() { return dynamic_cast<SplitOpState *>(op_state); }
};


struct SynopsisOp: public Op 
{
  SynopsisOp() { name = "help"; description="Prints this help message";
                 op_state = new SynopsisOpState; }

  int doIt();
};

struct InfoOp : public Op
{
  InfoOp() { name = "info"; description = "Shows information on an NDS file";
             op_state = new InfoOpState; buildAllArgs(); }
  
  int doIt();
  void buildAllArgs();
  void filenameArg(const QString &filename);  
  InfoOpState *state() { return dynamic_cast<InfoOpState *>(op_state); }
};

struct RepairOp : public Op
{
  RepairOp() { 
    name = "repair"; 
    description = "Attempts to repair a corrupt NDS file";
    op_state = new RepairOpState; buildAllArgs(); 
  }
  
  int doIt();
  void buildAllArgs();
  void filenameArg(const QString &filename);  
  void outfileArg(const QString &outfile);
  RepairOpState *state() { return dynamic_cast<RepairOpState *>(op_state); }
};

static 
//Op *ops[N_OPS] = {  new SynopsisOp(), new SplitOp(), new InfoOp() };
Op *ops[] = { new SynopsisOp(), new SplitOp(), new InfoOp(), new RepairOp(), 0 };

static 
Op *DEFAULT_OP = ops[0];

int main (int argc, char *argv[]) 
{
  (void)argc; // suppress compiler warnings..
  Op *op = 0; 
    
  argv++; /* consume argv[0] */

  QString arg = *argv; /* it's ok if *argv == 0 */

  arg.simplifyWhiteSpace(); // strip leading and trailing spaces
  
  for (Op **curr = ops; !op && *curr; curr++) {
    op = *curr;
    if (op->name == arg) {
      while (*(++argv)) {
#define EXIT_LOOP op = 0; break
        QString name = *argv, value;
                
        value = name.section('=', 1);
        name = name.section('=', 0, 0);

        name.simplifyWhiteSpace();
        value.simplifyWhiteSpace();        

        if (name.isEmpty() || value.isEmpty())  {
          cerr << "Invalid argument format encountered." << endl << endl;
          EXIT_LOOP; 
        }

        Op::ArgsMap_t::iterator it = op->allArgs.find(name);
        
        if ( it != op->allArgs.end() ) {
          Op::ArgCallback_t cb = it->second.second;
          (op->*cb)(value);
        } else {
          cerr << "Unknown argument encountered." << endl << endl;
          EXIT_LOOP;
        }
        
#undef EXIT_LOOP
      }
    } else op = 0;
  }

  if (!op) op = DEFAULT_OP; // if arg parsing fails, this op will be used
  
  int retval = EINVAL;

  if (op->state()->checkConstraints())   
    retval = op->doIt();
  else 
    retval = op->state()->constraintsError();

  return retval;
  
  /*
    todo: 
     1) pull off first arg
     2) find it in Op *ops[] array
     3) parse arguments according to that op
     4) if Op::state->checkConstraints(), call Op::doIt()
     5) otherwise call Op::constraintsError() and exit
  */
}

SplitOp::SplitOp()
{
    name = "split"; 
    description = 
      "Extract a portion of an NDS file and save it to another file.  "
      "Optionally,\nif the 'of=' parameter is a file ending in .bin, the "
      "output is a 'Binary Old\nDAQ Format' file.  A file ending in .txt or "
      ".dat, produces output as ASCII\ntext.\n";
    op_state = new SplitOpState; 
    buildAllArgs(); 
}

/* An internal class to deal with writing to .txt, .nds, or .bin format files.
   Auto-senses the file type based on the extention and saves the file
   appropriately. */
class TxtOrBinOrNDSWriter : protected TempSpoolerGZ<SampleStruct> {
public:
  TxtOrBinOrNDSWriter(const QString & file, uint rate, DSDStream::FileDataType t);
  ~TxtOrBinOrNDSWriter();
  
  const char *mode() const; 
  
  void writeSample(const SampleStruct *);
  void writeSample(const SampleStruct &s) { writeSample(&s); };
  void finish();
private:
  friend struct TMP;
  enum Mode { NDS, BIN = 1, ASCII = 2  };
  Mode m;
  uint rate;
  DSDOStream *dout;
  FILE       *fout/*, *tmp*/;
  QTextStream *t;

  map<uint,float> chans;

  bool done;

  void writeScan(scan_index_t index);
  void writeScanText(scan_index_t index);
};

TxtOrBinOrNDSWriter::TxtOrBinOrNDSWriter(const QString & file, 
                                         uint rate, 
                                         DSDStream::FileDataType t)
  : rate(rate), dout(0), fout(0), /*tmp(0),*/ t(0), done(false)
{
  if (((m = BIN) && file.endsWith(".bin")) 
      || ((m = ASCII) && (file.endsWith(".txt") || file.endsWith(".dat")))) {
    fout = fopen(file.latin1(), "w");
    if (!fout) {
      throw FileException(QString("Could not open output file %1.").arg(file), 
                          QString("Error is: %1").arg(strerror(errno)));
    }
  } else {
    dout = new DSDOStream(file, rate, t);
    m = NDS;
  }
}

const char * TxtOrBinOrNDSWriter::mode() const
{
  switch(m) {
  case BIN:
    return "Raw Binary";
  case ASCII:
    return "ASCII Text";
  case NDS:
    return "NDS Format (new DAQ System)";
  default:
    return "Unknown";
  }
}


  struct TMP 
  {
    typedef void (TxtOrBinOrNDSWriter::*FN)(scan_index_t);

        TMP(TxtOrBinOrNDSWriter *w,FN f) 
          : instance(w), index(0), i(0), chans(w->chans), fn(f)
        {}
        ~TMP() { done(); }
        void operator()(const SampleStruct & s) 
        {
          if (s.scan_index > index) { // new scan        
            index = s.scan_index;
            if (i) (instance->*fn)(s.scan_index);
          }
          chans[s.channel_id] = s.data;
          i++;
        }
        void done() { if (i) {(instance->*fn)(index); i = 0;} }
        TxtOrBinOrNDSWriter *instance;
        scan_index_t index, i;
        map<uint, float> &chans;
        FN fn;
  };

TxtOrBinOrNDSWriter::~TxtOrBinOrNDSWriter()
{
  if (!done) finish();
}

void TxtOrBinOrNDSWriter::finish()
{
  switch(m) {
  case NDS:
    dout->end(); 
    delete dout; 
    dout = 0; 
  break;
  case BIN:
    {
      int num_channels = chans.size();
      fwrite(&num_channels, sizeof(int), 1, fout);
      TMP oper(this, &TxtOrBinOrNDSWriter::writeScan);
      forEach(oper);
      oper.done();
      fclose(fout);
      fout = 0;
    }
  break;
  case ASCII:
    {
      t = new QTextStream(fout, IO_WriteOnly | IO_Truncate);
      TMP oper(this, &TxtOrBinOrNDSWriter::writeScanText);
      forEach(oper);
      oper.done();
      delete t;
      t = 0;
      fclose(fout);
      fout = 0;
    }
  break;
  }
  done = true;
}

void TxtOrBinOrNDSWriter::writeScan(scan_index_t index)
{
  map<uint,float>::iterator it, end;
  float data = index / static_cast<double>(rate);
  fwrite(&data, sizeof(float), 1, fout); // time
  for (it = chans.begin(), end = chans.end(); it != end; it++) {
    data = it->second;
    fwrite(&data, sizeof(float), 1, fout); // channel data
  } 
}

void TxtOrBinOrNDSWriter::writeScanText(scan_index_t index)
{
  double time = index / static_cast<double>(rate);
  (*t) << time;
  map<uint,float>::iterator it, end;
  for (it = chans.begin(), end = chans.end(); it != end; it++) 
    (*t) <<  " " << it->second;  
  (*t) << "\n";    
}

void TxtOrBinOrNDSWriter::writeSample(const SampleStruct *s)
{  
  if (m == NDS) { 
    dout->writeSample(s); 
  } else {
    spool(s, 1);
    chans[s->channel_id] = 0;
  }
}

int SplitOp::doIt()
{
  cerr << "Reading " << state()->infile.latin1() << endl;

  try {
    vector<SampleStruct> v;
    vector<SampleStruct>::iterator it;

    DSDIStream in(state()->infile);
    
    /* we need to do this due to bug in some nds format where startIndex() 
       is always zero!! */
    if (!in.readNextScan(v)) 
      { cerr << "No scans in input file!" << endl;  return EINVAL; }
  
   
    TxtOrBinOrNDSWriter out(state()->outfile, in.rateAt(state()->start), in.dataType());

    scan_index_t 
      i = 0, 
      real_start = ( v.size() ? v[0].scan_index : in.scanIndex() );

    in.setInFile(state()->infile); // reopen it now that we know the real start

    cerr << "Splicing out "  << Convert(state()->count).sStr() << " scans" 
         << endl
         << "Starting at index " << Convert(real_start).sStr() << endl
         << "Output file is " << state()->outfile.latin1() << endl;

    while(state()->count && in.readNextScan(v)) {
      if (i++ >= state()->start) {
        state()->count--;
        for (it = v.begin(); it != v.end(); it++) out.writeSample(&(*it));
      }
    } 
    
    cerr << "Repackaging as " << out.mode() << ".." << endl;
    
    out.finish();

    cerr << "Done!" << endl;
  } catch (Exception & e) {
    e.showConsoleError();
    return EIO;
  }
  return 0;
}

void SplitOp::buildAllArgs()
{

  allArgs[QString("if")] =
    ArgsMapValue_t(QString("Input file -- .nds file to read from (required)"),
                   (ArgCallback_t)&SplitOp::infileArg);

  allArgs[QString("of")] =
    ArgsMapValue_t(QString("Output file -- nds/bin/dat/txt file to write to (required)"),
                   (ArgCallback_t)&SplitOp::outfileArg);

  allArgs[QString("start")] =
    ArgsMapValue_t(QString("Start index -- relative scan to start from "
                           "(default: 0, or beginning)"),
                   (ArgCallback_t)&SplitOp::startArg);  

  allArgs[QString("count")] =
    ArgsMapValue_t(QString("Scan count -- the number of scans to copy "
                           "(defaults to all until end)"),
                   (ArgCallback_t)&SplitOp::countArg);  
  
}


void SplitOp::infileArg(const QString &infile) 
{
  state()->infile = infile;
}
void SplitOp::outfileArg(const QString &outfile)
{
  state()->outfile = outfile;
}

void SplitOp::startArg(const QString &start)
{
  bool ok;
  uint64 n = cstr_to_uint64(start.latin1(), &ok);
  if (ok) state()->start = n;
}

void SplitOp::countArg(const QString &count)
{
  bool ok;
  uint64 n = cstr_to_uint64(count.latin1(), &ok);
  if (ok) state()->count = n;
}

void InfoOp::buildAllArgs()
{
  allArgs["if"] = ArgsMapValue_t(QString("Input file to examine (required)"),
                                 (ArgCallback_t)&InfoOp::filenameArg);
}

void InfoOp::filenameArg(const QString &filename)
{
  state()->filename = filename;
}

int InfoOp::doIt()
{
  DSDIStream in(state()->filename);
  bool compensate_for_start_quirk = false; 

  try {
    in.start(); /* Potential exception here -- */
    vector<SampleStruct> dummy;
    in.readNextScan(dummy);
    if (in.startIndex() == 0 && in.scanIndex() != 1) 
      compensate_for_start_quirk = true;
  } catch (Exception & e) {
    e.showConsoleError();   /* will generate its own error message */
    return EINVAL;
  }
        
  scan_index_t
    startI = compensate_for_start_quirk ? in.scanIndex() :in.startIndex();
         
  std::string  
    startIndex = Convert(startI).sStr(),
    endIndex = Convert(in.endIndex()).sStr(),
    scanCount = Convert(in.scanCount()).sStr(),
    samplingRate = Convert((uint64)in.rateAt(startI)).sStr();
  double fileTime = in.timeAt(in.endIndex()-1) - in.timeAt(startI);

  bool hasDroppedScans = in.scanCount() < in.endIndex() - startI;

  cout << "Information for file '" << state()->filename.latin1() << "':" 
       << endl
       << "File size:               " << QFile(state()->filename).size() 
       << " bytes" <<  endl
       << "Starting scan index:     " << startIndex.c_str() << endl
       << "Ending scan index:       " << endIndex.c_str()   << endl
       << "Number of Scans:         " << scanCount.c_str()  
       << (hasDroppedScans ? " (file has holes/dropped scans)" : "") << endl
       << "Sampling rate:           " << samplingRate.c_str() << " Hz" << endl
       << "Time-length:             " << fileTime << " seconds" << endl;
  
  
  return 0; /* success */
}

void RepairOp::buildAllArgs()
{
  allArgs["if"] = ArgsMapValue_t(QString("File to recover (required)"),
                                 (ArgCallback_t)&RepairOp::filenameArg);
  allArgs["of"] = ArgsMapValue_t(QString("Recovered output file "
                                         "(defaults to RECOVERED.nds)"),
                                 (ArgCallback_t)&RepairOp::outfileArg);
}

void RepairOp::filenameArg(const QString &filename)
{
  state()->filename = filename;
}

void RepairOp::outfileArg(const QString &filename)
{
  state()->outfile = filename;
}

int RepairOp::doIt()
{

  try {
    
    DSDRStream in(state()->filename);
    DSDOStream out(state()->outfile, 1000);


    vector<SampleStruct> scan;

  
    while (in.readNextScan(scan)) {
      vector<SampleStruct>::iterator it;
      
      for (it = scan.begin(); it != scan.end(); it++) 
        out.writeSample(&(*it));
    }

    out.end();
    cout << "Recovered " << (uint64_to_cstr(out.scanCount())) << " samples." 
         << endl;
    return 0;

  } catch (Exception & e) {
    e.showConsoleError();
    return EINVAL;
  }
}

int SplitOpState::constraintsError() const
{
  cerr << "A required argument to 'split' is missing. " << endl
       << "You need to specify valid input and output files." 
       << endl;
  return EINVAL;
}


bool SplitOpState::checkConstraints() 
{
  return  QFile::exists(infile) && !outfile.isEmpty();
}

bool InfoOpState::checkConstraints()
{
  return QFile::exists(filename);
}

int  InfoOpState::constraintsError() const
{
  cerr << "A required argument to 'info' is missing. " << endl
       << "You need to specify a valid NDS file." 
       << endl;
  return EINVAL;  
}


bool RepairOpState::checkConstraints()
{
  return QFile::exists(filename) && !outfile.isEmpty();
}

int  RepairOpState::constraintsError() const
{
  cerr << "A required argument to 'repair' is missing. " << endl
       << "You need to specify a valid NDS file." 
       << endl;
  return EINVAL;  
}

int SynopsisOp::doIt()
{
  return state()->constraintsError();
}

int SynopsisOpState::constraintsError() const
{
  
  cerr << endl 
       << "NDSTool v0.1 by Calin A. Culianu <calin@ajvar.org>" << endl
       << "(NDS file format utility program)" << endl << endl
       << "Synopsis: " << endl << endl 
       << "    ndstool operation [arg=value ...]" << endl;

  /* detailed synopsis below... */
  /* build this dynamically based on descriptions */
  cerr << endl 
       << "Descriptions of available operations:" << endl;

  for (Op **curr = ops; *curr; curr++) {
    Op *op = *curr;
    /* print option name */
    cerr << endl 
         << "----------------------------------------------------------------------------" 
         << endl << op->name.latin1() << endl
         << "----------------------------------------------------------------------------" 
         << endl
         << op->description.latin1() << endl << endl
         << "  Possible Arguments: " << endl;
    
    Op::ArgsMap_t::iterator it = op->allArgs.begin();

    if (it == op->allArgs.end()) { cerr << "    (No arguments.)" << endl; }

    /* print arg descriptions.. */
    for ( ; it != op->allArgs.end(); it++) {      
      cerr << "    " << it->first.latin1() << "=(something)" << endl
           << "        " << it->second.first.latin1() << endl;
    }
    cerr << endl;
  }
  return EINVAL;
}
