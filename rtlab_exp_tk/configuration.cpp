/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (c) 2001 David Christini, Lyuba Golub, Calin Culianu
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

#include <qstring.h>
#include <qlayout.h>
#include <qdialog.h>
#include <qfiledialog.h>
#include <qsize.h>
#include <qgroupbox.h>
#include <qvgroupbox.h>
#include <qhgroupbox.h>
#include <qhbox.h>
#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qaccel.h>
#include <qtable.h>
#include <qheader.h>
#include <qmessagebox.h> 
#include <qspinbox.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <iostream>
#include <vector>
#include <comedi.h>

#include "configuration.h"
#include "daq_settings.h"
#include "probe.h"
#include "comedi_device.h"
#include "output_file_w.h"
#include "daq_help_browser.h"
#include "daq_mime_sources.h"
#include "daq_images.h"
#include "version.h"

#ifdef TEST_CW
#include <qapplication.h>
#include <qnamespace.h>
int 
main(int argc, char *argv[], char *envp[])
{
  try {
    Probe probe(true);
    DAQSettings settings;
  
    QApplication a(argc, argv);
    ConfigurationWindow w(&probe, &settings, 0, "Configuration Window", Qt::WType_TopLevel | Qt::WType_Modal);
    a.setMainWidget(&w);
    int ret = w.exec();
    cout << "Selected: " << w.deviceTable.selectedDevice().filename << endl;
    cout << "QDialog returned: " << ret << endl;
    return ret;
  } catch (NoComediDeviceException e) {
    cerr << "No comedi devices found, please install comedi and/or setup your "
            "DAQ board correctly." << endl;
    return(1);
  }

}

#endif

class AdvancedOptionsWindow : public QDialog
{ 
    typedef class ConfigurationWindow PARENTCLASS;
    friend class PARENTCLASS;
    AdvancedOptionsWindow (PARENTCLASS *parent);
     
    QSpinBox *samplingRateSpinner;    

    void addToolTipsAndWhatsThis();

};

AdvancedOptionsWindow::AdvancedOptionsWindow(PARENTCLASS *parent)
  : QDialog(parent, "Advanced Options Window", true)
{
  hide();
  setCaption("Advanced Options");
  QGridLayout *lo = new QGridLayout(this, 2, 1);
  QVGroupBox *gbox = 
    new QVGroupBox(caption(), this, "Advanced Options QVGroupBox");
  lo->addWidget(gbox, 0, 0);

  QHBox * tmphbox = new QHBox(gbox, "Adv. Opts HBox 0");

  (void)new QLabel("Sampling rate (Hz): ", tmphbox);
  
  samplingRateSpinner 
    = new QSpinBox(MIN_SAMPLING_RATE_HZ, MAX_SAMPLING_RATE_HZ, 1, tmphbox,
                   "Advanced Options Window Sampling Rate Spinbox");

  tmphbox = new QHBox(this, "Adv. Opts ButtonBox");
  lo->addWidget(tmphbox, 1, 0);
  
  QPushButton *ok, *cancel;

  ok = new QPushButton("Ok", tmphbox);
  cancel = new QPushButton("Cancel", tmphbox);
  ok->setMaximumSize(ok->sizeHint());
  cancel->setMaximumSize(cancel->sizeHint());

  connect(ok, SIGNAL(clicked(void)), this, SLOT(accept(void)));
  connect(cancel, SIGNAL(clicked(void)), this, SLOT(reject(void)));

  addToolTipsAndWhatsThis();
}


void AdvancedOptionsWindow::addToolTipsAndWhatsThis() 
{
  // TODO Add more tooltips/whatsthis here
}

ConfigurationWindow::ConfigurationWindow (const Probe &deviceProbe,
                                          DAQSettings &settings,
                                          QWidget *parent, 
                                          const char *name, 
                                          WFlags f) 
  : 
  QDialog(parent, name, f), 

  startupScreenSemantics(false),
  deviceProbe(deviceProbe), 
  settings(settings),
  masterGrid(this, 6, 1), 
  deviceSelectionGroup(1, Horizontal, this),
  templateSelectionGroup(1, Horizontal, this),
  outputFileSelectionGroup(1, Horizontal, this),
  deviceGroupContainer(&deviceSelectionGroup),
  templateGroupContainer(&templateSelectionGroup),
  outputFileGroupContainer(&settings, &outputFileSelectionGroup),
  deviceSelectionGrid(&deviceGroupContainer, 4, 3),
  templateSelectionGrid(&templateGroupContainer, 3, 2),
  deviceRadio(&deviceGroupContainer),
  fileRadio(&deviceGroupContainer),
  deviceTable(deviceProbe.probed_devices, &deviceGroupContainer),
  inputFile(&deviceGroupContainer),
  templateFile(&templateGroupContainer),
  browseInputFiles("Browse...", &deviceGroupContainer),
  browseLogTemplates("Browse...", &templateGroupContainer),
  logPreviewerLabel("Log Template Preview:", &templateGroupContainer),
  logPreviewer(&templateGroupContainer),
  showDialogOnStartupChk(this),
  bottomButtons(this),
  OK("Ok", &bottomButtons),
  helpButton("Help", &bottomButtons),
  advancedButton("Advanced...", &bottomButtons),
  adv(new AdvancedOptionsWindow(this))
{
   
  setIcon(QPixmap(DAQImages::configuration_img));
  setSizeGripEnabled(true);
  setMinimumSize(QSize(150,100));
  
  deviceSelectionGroup.setTitle("Input Device Selection");
  
  masterGrid.addWidget(&deviceSelectionGroup, 0, 0);
  deviceSelectionGrid.addMultiCellWidget(&deviceRadio, 0, 0, 0, 1);
  deviceSelectionGrid.addMultiCellWidget(&fileRadio, 2, 2, 0, 1);
  deviceSelectionRadioGroup.insert(&deviceRadio, 0);
  deviceSelectionRadioGroup.insert(&fileRadio, 1);
  deviceRadio.setText("Input data comes from a comedi device:");
  fileRadio.setText("Input data comes from a binary file:");
  connect(&fileRadio, SIGNAL(toggled( bool )), 
          &browseInputFiles, SLOT(setEnabled( bool )));
  connect(&fileRadio, SIGNAL(toggled( bool )), 
          &inputFile, SLOT(setEnabled( bool ))); 
  connect(&deviceRadio, SIGNAL(toggled( bool )),
          &deviceTable, SLOT(setEnabled( bool )));
  connect(&browseInputFiles, SIGNAL(clicked(void)),
          this, SLOT(askUserForInputFilename(void)));
  connect(&deviceRadio, SIGNAL(toggled(bool)),
          &outputFileGroupContainer, SLOT(setEnabled(bool)));
  deviceRadio.setChecked(true);
  deviceSelectionRadioGroup.hide();
  deviceSelectionGrid.addMultiCellWidget(&deviceTable, 1, 1, 1, 2);
  deviceSelectionGrid.addWidget(&inputFile, 3, 1);
  inputFile.setReadOnly(true);
  deviceSelectionGrid.addWidget(&browseInputFiles, 3, 2);
  browseInputFiles.setEnabled(fileRadio.isOn());
  inputFile.setEnabled(fileRadio.isOn());
  deviceTable.setEnabled(deviceRadio.isOn());
  fileRadio.setEnabled(deviceRadio.isOn());

  outputFileGroupContainer.setEnabled(deviceRadio.isOn());
  outputFileSelectionGroup.setTitle("Output File Selection");
  masterGrid.addWidget(&outputFileSelectionGroup, 1, 0);

  templateSelectionGroup.setTitle("Log Template Selection");

  masterGrid.addWidget(&templateSelectionGroup, 2, 0);
  templateSelectionGrid.addWidget(&templateFile, 0, 0);
  templateFile.setReadOnly(true);
  templateSelectionGrid.addWidget(&browseLogTemplates, 0, 1);
  connect(&browseLogTemplates, SIGNAL(clicked(void)),
          this, SLOT(askUserForTemplateFilename(void)));

  templateSelectionGrid.addMultiCellWidget(&logPreviewerLabel, 1, 1, 0, 1);
  templateSelectionGrid.addMultiCellWidget(&logPreviewer, 2, 2, 0, 1); 

  masterGrid.addWidget(&showDialogOnStartupChk, 3, 0); 
  masterGrid.addWidget(&bottomButtons, 4, 0);
  //masterGrid.addWidget(&OK, 4, 0);
  QLabel *tmplbl = new QLabel(QString("v.%1").arg(VERSION_NUM_STR), this);
  tmplbl->setAlignment(AlignRight);
  masterGrid.addWidget(tmplbl, 5, 0);
  showDialogOnStartupChk.setText("Always show this configuration window on "
                                 "application startup");
  OK.setAutoDefault(true);
  OK.setMaximumSize(OK.sizeHint());
  connect(&OK, SIGNAL(clicked(void)),
          this, SLOT(accept(void)));
  
  helpButton.setMaximumSize( helpButton.sizeHint() );
  connect(&helpButton, SIGNAL(clicked(void)),
          this, SLOT(configHelp(void)));

  advancedButton.setMaximumSize( advancedButton.sizeHint() );
  connect(&advancedButton, SIGNAL(clicked(void)),
          this, SLOT(advancedDialog(void)));

  fromSettings();

  addToolTipsAndWhatsThis();
}

void ConfigurationWindow::addToolTipsAndWhatsThis()
{
  QToolTip::add(&advancedButton,"Open advanced options (sampling rate, etc.)");
  QWhatsThis::add(&advancedButton, 
                  "Opens the advanced options window.  "
                  "Not recommended for beginners!");
}

void
ConfigurationWindow::show()
{
  fromSettings(); /* sets up default selected values in this dialog
                     based on stuff from the Settings instance */
  QDialog::show();
}

void
ConfigurationWindow::fromSettings()
{
  /* todo: rewrite settings :/ */
  templateFile.setText(settings.getTemplateFileName());
  logPreviewer.previewUrl("file://" + templateFile.text());

  inputFile.setText(settings.getFileSourceFileName()); 
  if (settings.getInputSource() == 
      DAQSettings::File) {
    fileRadio.setChecked(true);
  } else {
    deviceRadio.setChecked(true);
  }

  outputFileGroupContainer.fromSettings();

  const QString & settingsDevF = settings.getDevice();
  QListViewItem *item = deviceTable.firstChild();
  while (item != NULL) {
    if (settingsDevF == item->text(0)) {
      deviceTable.setSelected(item, true);
    }
    item = item->nextSibling();
  }
  
  showDialogOnStartupChk.setChecked(settings.getShowConfigOnStartup());

  adv->samplingRateSpinner->setValue(settings.samplingRateHz());

}

void
ConfigurationWindow::askUserForInputFilename()
{
  static const char *filters[2] = {"All files (*)", 0};
  QFileDialog fileDialog(this, 0, true);

  fileDialog.setMode(QFileDialog::ExistingFile);
  fileDialog.setViewMode(QFileDialog::Detail);
  fileDialog.setFilters(filters);

  fileDialog.setSelection(inputFile.text());

  if (fileDialog.exec()) {
    inputFile.setText(fileDialog.selectedFile());
  }
}

void
ConfigurationWindow::askUserForTemplateFilename()
{

  static const char *filters[3] 
    = {"DAQ Log Files (*.log)", "All files (*)", 0};
  QFileDialog fileDialog(this, 0, true);

  fileDialog.setMode(QFileDialog::ExistingFile);
  fileDialog.setViewMode(QFileDialog::Detail);
  fileDialog.setFilters(filters);

  TextContentsPreviewer cp(&fileDialog);

  fileDialog.setSelection(templateFile.text());
  fileDialog.setContentsPreviewEnabled(true);
  fileDialog.setContentsPreview(&cp, &cp);
  fileDialog.setPreviewMode(QFileDialog::Contents);


  if (fileDialog.exec()) {
    templateFile.setText(fileDialog.selectedFile());
  }

  logPreviewer.previewUrl("file://" + templateFile.text());
}

void 
ConfigurationWindow::toSettings()
{
  settings.setDevice(deviceTable.selectedDevice().filename);
  settings.setTemplateFileName(templateFile.text());
  settings.setFileSourceFileName(inputFile.text());
  
  { 
    DAQSettings::InputSource is = DAQSettings::File;
    if (deviceRadio.isChecked()) {
      is = ( selectedDevice().find(ComediSubDevice::AnalogInput)
             .used_by_rt_process /* <-- we assume that find() didn't return 
                                    a null comedi subdevice here! */
             ? DAQSettings::RTProcess
             : DAQSettings::Comedi );
    }	
    settings.setInputSource( is ); 
  }
  
  outputFileGroupContainer.toSettings();

  settings.setShowConfigOnStartup(showDialogOnStartupChk.isChecked()); 

  settings.setSamplingRateHz(adv->samplingRateSpinner->value());
}

void
ConfigurationWindow::accept()
{
  if (!validate()) return;
  toSettings();
  settings.saveSettings();
  QDialog::accept();
}

void
ConfigurationWindow::configHelp()
{
  DAQHelpBrowser::getDefaultBrowser()->
    openPage( DAQMimeSources::HTML::configWindow );
}

void
ConfigurationWindow::advancedDialog()
{
  sampling_rate_t saved_rate = settings.samplingRateHz();

  if (adv->exec() != QDialog::Accepted)
    adv->samplingRateSpinner->setValue(saved_rate);
}

bool
ConfigurationWindow::validate()
{
  bool retval = true;

  if (!startupScreenSemantics)
    retval = 
      QMessageBox::information(this,
                               "Changes will take effect later",
                               "Settings changes (if any), will take effect "
                               "the next time the application is restarted.",
                               QMessageBox::Ok);

  return retval; /* always true for now... 
                    put other stuff in here if we want some sort of 
                    all-stop-on-illegal inputs.. */
}

ConfigurationWindow::DeviceListView::
DeviceListView (const vector<ComediDevice> & v, 
                QWidget * parent, 
                const char * name)  : QListView (parent, name),  devs(v)
{ 
  setSelectionMode(Single);
  addColumn("Device");      
  addColumn("Board Name");
  addColumn("Driver");
  addColumn("No. AI Channels");
  
  for (uint i = 0; i < devs.size(); i++) {
    const ComediDevice & d = devs[i];
    const ComediSubDevice & ai = d.find(ComediSubDevice::AnalogInput);
    QString driverTxt ( (!ai.isNull() && ai.used_by_rt_process  
                         ? QString("rtab.o")
                         : d.drivername) );
    QString no_ai_chan ( (ai.isNull() 
                          ? QString("No Output Device Detected") 
                          : QString().setNum(ai.n_channels)));
    
    insertItem(new QListViewItem(this, d.filename, d.devicename, driverTxt, 
                                 no_ai_chan));
  }
  
  /* setup the table header ... */
  QHeader *h = header();
  h->setClickEnabled( false );
  h->setResizeEnabled( true );
  h->setMovingEnabled( false );      
  
}

const ComediDevice &
ConfigurationWindow::DeviceListView::selectedDevice() const
{
  return devs[selectedItem()->itemPos()];
}

ConfigurationWindow::TextContentsPreviewer::
TextContentsPreviewer ( QWidget *w,  const char *n ) : QTextView(w, n)
{
  /* nothing */
}

void
ConfigurationWindow::TextContentsPreviewer::previewUrl(const QUrl & url)
{
  if (!url.isLocalFile() || !url.isValid()) {
    return;
  }

  QFile file(url.path());
  int size;
  if (file.open(IO_ReadOnly) && (size = file.size()) > 0) {
    char *buf = new char[size+1];
    int n_read = file.readBlock(buf, size);
    buf[n_read] = 0;
    this->setText(buf);
    delete buf;
  } else {
    this->setText(QString::null);
  }
  file.close();
}

