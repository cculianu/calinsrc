/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 2002 David Christini, Calin Culianu
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see COPYRIGHT file); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA, or go to their website at
 * http://www.gnu.org.
 */
#include <vector>

#include <qwidget.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qfont.h>
#include <qstring.h>
#include <qgroupbox.h>
#include <qtimer.h>
#include <qcheckbox.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qhbox.h>
#include <qvbox.h>
#include <qlineedit.h>
#include <qvalidator.h>
#include <qmenubar.h>
#include <qpopupmenu.h>
#include <qmessagebox.h>
#include <qfiledialog.h>
#include <qfile.h>
#include <qtextstream.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "daq_system.h"
#include "shm_mgr.h"
#include "ecggraph.h"
#include "plugin.h"
#include "exception.h"

#include "tweaked_mbuff.h"
#include "avn_stim.h"

#include "avn_stim_private.h"

static const int MAX_DATA_VECTOR_SIZE = 1000000;

class SearchableComboBox : public QComboBox
{
public:
  SearchableComboBox ( QWidget * parent = 0, const char * name = 0 );
  SearchableComboBox( bool rw, QWidget * parent = 0, const char * name = 0 );
  int findItem(const QString & text, int match_criteria);
  void setSelected(int index, bool this_param_is_ignored = false) 
  { setCurrentItem(index); }
};

SearchableComboBox::SearchableComboBox ( QWidget * parent, const char * name)
  : QComboBox ( parent, name ) {}
SearchableComboBox::SearchableComboBox ( bool rw, QWidget * parent, const char * name)
  : QComboBox ( rw, parent, name ) {}

int SearchableComboBox::findItem(const QString & str, int criteria)
{
  QString txt = (criteria & CaseSensitive ? str : str.lower()) ;
  
  for (uint i = 0; i < count(); i++) {
    QString item = (criteria & CaseSensitive ? text(i) : text(i).lower());

    if (   
           criteria & Contains   && item.contains(txt) 
        || criteria & BeginsWith && item.startsWith(txt) 
        || criteria & EndsWith   && item.endsWith(txt)
        || criteria & ExactMatch && item == txt
       ) 
      return i;
  }
  return -1;
}

extern "C" {

  /* Stuff needed by plugin engine... */
  int ds_plugin_ver = DS_PLUGIN_VER;

  const char * name = "AV Node Control Experiment",
             * description =
              "Some GUI controls for interfacing with the AV Node stimulation "
              "real-time module named 'avn_stim.o'.  This rtl/rt_process "
              "addon was developed for an experiment in the Cardiac "
              "Electrodynamics Laboratory of  Cornell University.",
             * author =
              "David J. Christini, Ph.D and Calin A. Culianu.",
             * requires =
              "avn_stim.o be installed into the kernel.  Analog output.";
  
  Plugin * entry(QObject *o) 
  { 

    /* Top-level widget.. parent is root */
    DAQSystem *d = dynamic_cast<DAQSystem *>(o);

    if (!d) throw PluginException ("AVN Stim Plugin Load Error", "The AVN Stim Plugin can only be used in conjunction with daq_system!  Sorry!");    
    return new AVNStim(d); ;
  } 

};

static const int beats_per_gridline = 100;
static const int num_gridlines = 5;
/* default values for the graphs */
static const double rr_min   = 0.0,
                    rr_max   = 2000.0,
                    stim_min = 0.0,
                    stim_max = 50.0,
                    g_min    = -15.0,
                    g_max    = 15.0;

#define data (reinterpret_cast<vector<AVNLiebnitz> *>(this->__data))

AVNStim::AVNStim(DAQSystem *daqSystem_parent) 
  : QObject(daqSystem_parent, ::name), need_to_save(false), __data(NULL), 
    shm(NULL), in_fifo(-1), out_fifo(-1)
{
  data = new vector<AVNLiebnitz>;

  moduleAttachDetach(); /* can throw exception here */

  /* stub.. */
  window = new QWidget (0, name(), Qt::WStyle_Tool);
  window->setCaption(name());

  masterlayout = new QGridLayout(window, 2, 2);

  QMenuBar *mb = new QMenuBar(window);
  masterlayout->addMultiCellWidget(mb, 0, 0, 0, 1);

  QPopupMenu *fileMenu = new QPopupMenu(mb, "File");
  mb->insertItem("&File", fileMenu);
  fileMenu->insertItem("&Save", this, SLOT ( save() ) );
  fileMenu->insertItem("Save&As", this, SLOT ( saveAs() ) );

  /* graphs.. */
  graphs = new QWidget(window);
  masterlayout->addWidget(graphs, 1, 1);
  masterlayout->setColStretch(1, 2);

  graphlayout = new QGridLayout(graphs, 6, 2);
  graphlayout->setColStretch(1, 1);
  
  rr_graph   = new ECGGraph (beats_per_gridline, num_gridlines, 
                             rr_min, rr_max, 
                             graphs, QString(name()) + " - RR Interval");
  stim_graph = new ECGGraph (beats_per_gridline, num_gridlines, 
                             stim_min, stim_max, 
                             graphs, QString(name()) + " - Number of Stimuli");
  
  g_graph    = new ECGGraph (beats_per_gridline, num_gridlines, 
                             g_min, g_max,
                             graphs, QString(name()) + " - G Values");

  g_graph->plotFactor = stim_graph->plotFactor = rr_graph->plotFactor = 2;

  graphlayout->addWidget(rr_graph, 1, 1); graphlayout->setRowStretch(1, 3);
  graphlayout->addWidget(stim_graph, 3, 1); graphlayout->setRowStretch(3, 3);
  graphlayout->addWidget(g_graph, 5, 1); graphlayout->setRowStretch(5, 3);
  

  graphlayout->addWidget(new QLabel("RR Graph", graphs), 0, 1);
  graphlayout->addWidget(new QLabel("Stim Graph", graphs), 2, 1);
  graphlayout->addWidget(new QLabel("G Graph", graphs), 4, 1);

  addAxisLabels();
  /* end graphs... */

  /* controls... */

  controls = new QWidget(window);
  masterlayout->addWidget(controls, 1, 0);

  const int n_ctl_r = 3, n_ctl_c = 1;
  controlslayout = new QGridLayout(controls, n_ctl_r, n_ctl_c);

  { /* stats groupbox */
    QGroupBox *stats = new QGroupBox(1, Vertical, "Stats", controls);
    controlslayout->addMultiCellWidget(stats, 0, 0, 0, n_ctl_c-1);

    QWidget *tmpw = new QWidget(stats);

    QGridLayout *tmplo = new QGridLayout(tmpw, 5, 2);

    tmplo->addWidget(new QLabel("RR Interval (ms): ", tmpw), 0, 0);
    tmplo->addWidget(new QLabel("RR Target (ms): ", tmpw), 1, 0);
    tmplo->addWidget(new QLabel("Number of Stims: ", tmpw), 2, 0);
    tmplo->addWidget(new QLabel("Value of g: ", tmpw), 3, 0);
    tmplo->addWidget(new QLabel("Last beat scan index: ", tmpw), 4, 0);
    
    tmplo->addWidget(current_rri = new QLabel(tmpw), 0, 1); 
    tmplo->addWidget(current_rrt = new QLabel(tmpw), 1, 1); 
    tmplo->addWidget(current_stim = new QLabel(tmpw), 2, 1);
    tmplo->addWidget(current_g   = new QLabel(tmpw), 3, 1);
    tmplo->addWidget(last_beat_index = new QLabel(tmpw), 4, 1);

  }
  
  { /* controls groupbox */
    QGroupBox *ctls = new QGroupBox(1, Vertical, "Controls", controls);
    controlslayout->addMultiCellWidget(ctls, 1, 1, 0, n_ctl_c-1);

    QVBox *tmpvb = new QVBox(ctls);

    QHBox *tmphb = new QHBox(tmpvb);

    (void) new QLabel("AI Channel to Monitor:", tmphb);
    ai_channels = new SearchableComboBox(tmphb);
    connect(ai_channels, SIGNAL(activated(const QString &)), this, SLOT(changeAIChan(const QString &)));
    connect(daqSystem(), SIGNAL(channelOpened(uint)), this, SLOT(synchAIChan()));
    connect(daqSystem(), SIGNAL(channelClosed(uint)), this, SLOT(synchAIChan()));
    synchAIChan();

    (void) new QLabel("  RR Target:", tmphb);
    rr_target = new QSpinBox(static_cast<int>(rr_graph->rangeMin()),
                             static_cast<int>(rr_graph->rangeMax()),
                             1, tmphb);
    rr_target->setValue(static_cast<int>(shm->rr_target));
    connect(rr_target, SIGNAL(valueChanged(int)), this, SLOT(changeRRTarget(int)));
    

    /* the analog output-related controls */
    tmphb = new QHBox(tmpvb);        
    
    
    (void) new QLabel("AO Stim Channel:", tmphb);
    ao_channels = new SearchableComboBox(tmphb);
    (void) new QLabel("    " /* poor man's spaceing */, tmphb);

    populateAOComboBox();
    synchAOChan();
    connect(ao_channels, SIGNAL(activated(const QString &)), this, SLOT(changeAOChan(const QString &)));
       

    QCheckBox *stim_on_chk = new QCheckBox("Suppress Actual Stim", tmphb);
    stim_on_chk->setChecked(!shm->stim_on);
    connect(stim_on_chk, SIGNAL(stateChanged(int)), 
            this, SLOT(setFakeStim(int)));
        

    tmphb = new QHBox(tmpvb);

    (void) new QLabel("Value of g:", tmphb);
    gval = new QLineEdit(tmphb);
    gval->setValidator(new QDoubleValidator(gval));
    gval->setText(QString("%1").arg(shm->latest_snapshot.g_val));
    connect(gval, SIGNAL(textChanged(const QString &)), this, SLOT(changeG(const QString &)));
    (void) new QLabel("    ", tmphb); // po' man's spacing
    QCheckBox *g_adj_manual_only = new QCheckBox("Adjust g Only Manually", tmphb);
    g_adj_manual_only->setChecked(shm->g_adjustment_mode == AVN_G_ADJ_MANUAL);
    connect(g_adj_manual_only, SIGNAL(stateChanged(int)), this, SLOT(gAdjManualOnly(int)));
    
  }
  
  controlslayout->addMultiCellWidget // footnote at the bottom
    (new QLabel(QString("AVN Control Experiment Notes:\n"
                        "All graphs display %1 beats on the x-axis.\n"
                        "The vertical gridlines are %2 beats apart.")
               .arg(beats_per_gridline * num_gridlines)
               .arg(beats_per_gridline), controls), 
     n_ctl_r-1, n_ctl_r-1, 0, n_ctl_c-1, AlignBottom | AlignLeft);


  window_id = daqSystem()->windowMenuAddWindow(window);  
  window->show();


  timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(periodic()));
  timer->start(50);
}

AVNStim::~AVNStim() 
{
  timer->stop();
  safelyQuit(); /* prompts the user to save, if necessary.. kinda sloppy to 
                   put this in a destructor.. but 'oh well' */
  daqSystem()->windowMenuRemoveWindow(window_id);
  if (shm)  shm->spike_channel = -1; // to 'turn off' avn stim...
  if (!needFifo()) moduleAttachDetach();  
  delete window;
  delete data;
}

const char *
AVNStim::name() const
{
  return ::name;
}

const char *
AVNStim::description() const
{
  return ::description;
}


void AVNStim::moduleAttachDetach()
{
  if (shm == NULL) {
    shm = (AVNShared *)mbuff_attach(AVN_SHM_NAME, sizeof(struct AVNShared));
    if (!shm || shm->magic != AVN_SHM_MAGIC) 
      throw PluginException ("AVN Stim Plugin Attach Error", 
                             "The AVN Stim Plugin can not find the shared "
                             "memory buffer named \"" AVN_SHM_NAME "\".  "
                             "(Or it is of the wrong version)\n\n"
                             "Are you sure that module avn_stim.o is loaded?");
  } else {
    mbuff_detach(AVN_SHM_NAME, shm);
    shm = NULL;
  }

  if (needFifo() && shm) {
    QString fname;

    fname.sprintf("%s%d","/dev/rtf",shm->in_fifo_minor);
    in_fifo = open(fname, O_RDONLY | O_NDELAY);
    fname.sprintf("%s%d","/dev/rtf",shm->out_fifo_minor);
    out_fifo = open(fname, O_WRONLY);

    if (needFifo())
      throw PluginException ("AVN Stim Plugin Attach Error",
                             QString (
                             "Even though the AVN Stim Kernel Module is "
                             "loaded, the AVN Stim plugin could not attach\n "
                             "to the required fifo \"%1\".  Please make sure "
                             "this file exists and is read/write for the \n"
                             "current user."
                             ).arg(fname));

    /* empty the fifo to make sure we start with a clean slate */
    char buf[AVN_KERNEL_USER_FIFO_SZ];
    ::read(in_fifo, &buf, AVN_KERNEL_USER_FIFO_SZ);
    
  } else if (!needFifo()) {
    close(in_fifo); in_fifo = -1;
    close(out_fifo); out_fifo = -1;
  }
}

/* returns parent() dynamic_cast to DAQSystem* */
DAQSystem * AVNStim::daqSystem()  
{ return dynamic_cast<DAQSystem *>(parent()); }


/* Very long-winded code to build the graph axis labels... */
void AVNStim::addAxisLabels()
{
  /* add the axis labels */
  QGridLayout *tmp_lo;
  QWidget *w;
  QLabel *tmp_l; // temporary pointer to axis labels
  QFont f; f.setPointSize(8); // label point size

  /* RR Interval Axis Labels */
  w = new QWidget(graphs); // dummy widget to have nested grids
  graphlayout->addWidget(w, 1, 0);

  tmp_lo = new QGridLayout(w, 3, 1);

  tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(rr_graph->rangeMax()), w), 0, 0, AlignTop | AlignRight); 
  tmp_l->setFont(f);

  tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum((rr_graph->rangeMax() - rr_graph->rangeMin()) / 2.0 + rr_graph->rangeMin()), w), 1, 0, AlignVCenter | AlignRight);
  tmp_l->setFont(f);

  tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(rr_graph->rangeMin()), w), 2, 0, AlignBottom | AlignRight);
  tmp_l->setFont(f);

  /* Stim Graph Axis Labels */
  w = new QWidget(graphs); // dummy widget to have nested grids
  graphlayout->addWidget(w, 3, 0);

  tmp_lo = new QGridLayout(w, 3, 1);

  tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(stim_graph->rangeMax()), w), 0, 0, AlignTop | AlignRight); 
  tmp_l->setFont(f);

  tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum((stim_graph->rangeMax() - stim_graph->rangeMin()) / 2.0 + stim_graph->rangeMin()), w), 1, 0, AlignVCenter | AlignRight);
  tmp_l->setFont(f);

  tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(stim_graph->rangeMin()), w), 2, 0, AlignBottom | AlignRight);
  tmp_l->setFont(f);

  /* G Graph Axis Labels */
  w = new QWidget(graphs); // dummy widget to have nested grids
  graphlayout->addWidget(w, 5, 0);

  tmp_lo = new QGridLayout(w, 3, 1);

  tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(g_graph->rangeMax()), w), 0, 0, AlignTop | AlignRight); 
  tmp_l->setFont(f);

  tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum((g_graph->rangeMax() - g_graph->rangeMin()) / 2.0 + g_graph->rangeMin()), w), 1, 0, AlignVCenter | AlignRight);
  tmp_l->setFont(f);

  tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(g_graph->rangeMin()), w), 2, 0, AlignBottom | AlignRight);
  tmp_l->setFont(f);

}

void AVNStim::periodic()
{
  /* check the in fifo for data, and act upon it if found... */
  readInFifo();

  /* synch the ao_channels listbox with what is really in the kernel */
  synchAOChan();    

}
void AVNStim::readInFifo()
{
  static const int n2rd = 1000; /* number of AVNLiebnitz's to read at a time */
  AVNLiebnitz buf[n2rd];
  int n_read = 0, n_bufs = 0, i = 0;

  n_read = ::read(in_fifo, buf, sizeof(buf));

  n_bufs = n_read / sizeof(AVNLiebnitz);

  need_to_save = (n_bufs ? true : need_to_save);

  if (data->size() + n_bufs > MAX_DATA_VECTOR_SIZE) 
    data->erase(data->begin(), data->begin() + n_bufs); // free up some space 
  
  for (i = 0; i < n_bufs; i++) {
    /* todo: plot to graphs here... */
    rr_graph->plot(static_cast<double>(buf[i].rr_interval));
    stim_graph->plot(static_cast<double>(buf[i].stimuli));
    g_graph->plot(buf[i].g_val);
    data->push_back(buf[i]);
  }
  /* now update the stats once per read() call (meaning we take the
     last AVNLiebnitz we got */
  if (!n_bufs) return;
  
  current_rri->setText(QString("%1").arg(buf[n_bufs-1].rr_interval));
  current_rrt->setText(QString("%1").arg(shm->rr_target));
  current_stim->setText(QString("%1").arg(buf[n_bufs-1].stimuli));
  current_g->setText(QString("%1").arg(buf[n_bufs-1].g_val));
  last_beat_index->setText(QString(uint64_to_cstr(buf[n_bufs-1].scan_index)));    
  /* incomplete... please finish! - Calin */

}

void AVNStim::setFakeStim(int state)
{
  shm->stim_on = !state;
}

/* Possible race conditions here but it's too much trouble to implement
   atomicity for the free channels list right now... */
void AVNStim::populateAOComboBox()
{
  uint i;

  ao_channels->clear();

  for (i = 0; i < ShmMgr::numChannels(ComediSubDevice::AnalogOutput); i++)
    if (!ShmMgr::isChanOn(ComediSubDevice::AnalogOutput, i) || i == shm->ao_chan)
      ao_channels->insertItem(QString("%1").arg(i));
  
}

/* possible race conditions here.. since channel free list access is
   non-atomic! */
void AVNStim::changeAOChan(const QString & selected)
{
  int selected_i = selected.toInt();
  
  /* disconnect here to avoid infinite recursion */
  disconnect(ao_channels, SIGNAL(activated(const QString &)), this, SLOT(changeAOChan(const QString &)));
  populateAOComboBox(); /* make sure the channel still is open --
                          possible race condition here! */

  /* if it's still there, tell the kernel about our desire to change
     the channel */
  if (ao_channels->findItem(selected, CaseSensitive|ExactMatch) >= 0) {
    shm->ao_chan = selected_i;
    synchAOChan(); /* just for kicks - this may revert back for a split
                      second as synchAOChan() in periodic() */
  }
  connect(ao_channels, SIGNAL(activated(const QString &)), this, SLOT(changeAOChan(const QString &)));
}

/* synch the ao_channels listbox with what is really in the kernel */
void AVNStim::synchAOChan() 
{
  QString ao_chan;

  ao_chan.setNum(shm->ao_chan);

  if (ao_channels->currentText() != ao_chan) 
    ao_channels->setSelected(ao_channels->findItem(ao_chan, CaseSensitive|ExactMatch), true); 
}

void AVNStim::changeAIChan(const QString &chan)
{
  bool ok;

  shm->spike_channel = chan.toInt(&ok);

  if (!ok) shm->spike_channel = -1;
}

void AVNStim::synchAIChan()
{
  uint i;
  int sel;

  disconnect(ai_channels, SIGNAL(activated(const QString &)), this, SLOT(changeAIChan(const QString &)));

 ai_channels->clear();
  ai_channels->insertItem("Off");
  for (i = 0; i < ShmMgr::numChannels(ComediSubDevice::AnalogInput); i++)
    if (ShmMgr::isChanOn(ComediSubDevice::AnalogInput, i))
      ai_channels->insertItem(QString().setNum(i));
  
  sel = ai_channels->findItem(QString().setNum(shm->spike_channel), CaseSensitive | ExactMatch);
  if (sel < 0) {
    ai_channels->setSelected(0);
    shm->spike_channel = -1;
  }
  else ai_channels->setSelected(sel); 

  connect(ai_channels, SIGNAL(activated(const QString &)), this, SLOT(changeAIChan(const QString &)));

}

void AVNStim::changeRRTarget(int rrt_in_ms)
{
  shm->rr_target = rrt_in_ms;
}

void AVNStim::gAdjManualOnly(int yes)
{
  shm->g_adjustment_mode = (yes ? AVN_G_ADJ_MANUAL : AVN_G_ADJ_AUTOMATIC);
}

void AVNStim::changeG(const QString &g)
{  
  double newG;
  bool ok;

  newG = g.toDouble(&ok);
  if (ok)
    ::write(out_fifo, &newG, sizeof(newG));
}

void AVNStim::save()
{ 
  if (outFile.isNull()) { saveAs(); return; }

  QFile f(outFile);

  if (!f.open(IO_WriteOnly | IO_Translate | IO_Truncate)) {
    QMessageBox::critical(window, outFile + " cannot be opened!", 
                          QString("Could not open ") + outFile + " for writing"
                          ", please choose another file.", QMessageBox::Ok, 0);
    saveAs();
    return;
  }

  QTextStream out(&f);
  out.setEncoding(QTextStream::Latin1);

  out << "#Scan Index\tRR Interval\tNumber of Stimuli\tG\n";

  for (vector<AVNLiebnitz>::iterator i=data->begin(); i != data->end(); i++) {
    out << uint64_to_cstr(i->scan_index) << "\t" << i->rr_interval << "\t" 
        << i->stimuli << "\t" << i->g_val << "\n";
  }
  f.flush();
  if ( (f.status() & IO_Ok) != IO_Ok ) {
    Exception("Error writing to outfile", "There was an unspecified error "
              "while saving data to the output file.  "
              "Please choose another file.").showError();
    saveAs();
    return;
  }

  need_to_save = false;
}

void AVNStim::saveAs()
{
  bool dont_overwrite;

  do {
    outFile =  
      QFileDialog::getSaveFileName(outFile, QString::null, window, 
                                   "save file dialog",
                                   "Choose a file to which to save the"
                                   " AVN data" );

    if (outFile.isNull()) return; // user aborted

    if (QFile(outFile).exists())   
      dont_overwrite = 
        QMessageBox::warning(window, "File exists", 
                             outFile + " exists, overwrite?", 
                             QMessageBox::Yes, QMessageBox::No) == 1;
    else 
      dont_overwrite = false;
  } while (dont_overwrite);
  save();
}

void AVNStim::safelyQuit()
{
  if (need_to_save) {
    int usersaid = 
      QMessageBox::warning(window, "Data is unsaved", "The AVN Stim Plugin is terminating, and the data for the AVN Stim Control Experiment is unsaved.\nDo you wish to save or discard changes?", "Save Changes", "Discard Changes");
    if (usersaid == 0)
      save(); /* this way even on file pick error
                 they will be prompted to save */
  }
}

