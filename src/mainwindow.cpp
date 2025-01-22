#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QDebug>

#define POLL_TIMER 1000

/*
***************************************************************************************************************************************
* Constructor of the GUI
* Prepare the SerialPort object (SG_port), Setup periodic checking for available serial ports.
* Configure the GUI's state at the start of the program
***************************************************************************************************************************************
*/
MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	/* Create a new instance of the SG_port serialport object and connect its SerialPort::errorOccurred signal to our serialPort_error_handler slot function. */
	SG_port = new QSerialPort;
	connect(SG_port, &QSerialPort::errorOccurred, this, &MainWindow::serialport_error_handler);

	/* Set hard-coded defaults for the serialport */
	SG_port->setBaudRate(QSerialPort::Baud115200);
	SG_port->setDataBits(QSerialPort::Data8);
	SG_port->setParity(QSerialPort::NoParity);
	SG_port->setFlowControl(QSerialPort::NoFlowControl);
	SG_port->setStopBits(QSerialPort::OneStop);

	/* Set default portname for SG_port */
	update_port_list();													//Populate the list of available serial ports; fill the ports comboBox in the UI with options.
	on_comboBox_ports_activated(ui->comboBox_ports->currentText());		//Set the portname to the current value of the ports comboBox (can be changed later by user).

	/* Set up polling of COM ports and periodic updating of the ports comboBox
	 * Create a new instance of the port_poll_timer QTimer object and connect its QTimer::timeout signal to our update_port_list slot function. */
	port_poll_timer = new QTimer();
	connect(port_poll_timer, &QTimer::timeout, this, &MainWindow::update_port_list);

	/* Start the timer. Everytime it expires, the timeout() signal is emitted, which triggers the update_port_list slot function. */
	port_poll_timer->start(POLL_TIMER);

	/* Configure visuals correctly */
	ui->plainTextEdit->setTabStopDistance(20);	//Configure the Tab length in pixels for the commands log (just for nice aesthetics)
	show_connection_buttons(true);				//Enable the serial port management buttons
	show_main_buttons(false);					//Disable the messaging buttons
}

MainWindow::~MainWindow()
{
	delete ui;
}

/*
***************************************************************************************************************************************
* UI management and convenience functions
* Functions to enable/disable UI elements + Mathematical conversions between dBm and watt
***************************************************************************************************************************************
*/

/* Enable/Disable UI elements pertaining to establishing the serial connection to the Signal Generator board. */
void MainWindow::show_connection_buttons(bool enable)
{
	ui->comboBox_ports->setEnabled(enable);
	ui->pushButton_connect->setEnabled(enable);
	ui->pushButton_autoconnect->setEnabled(enable);
	ui->pushButton_disconnect->setEnabled(!enable);
}

/* Enable/Disable UI elements pertaining to sending commands to the signal generator board. */
void MainWindow::show_main_buttons(bool enable)
{
	ui->frame->setEnabled(enable);
}

/* Unit Conversion for convenience: dBm to watt */
double MainWindow::convert_dbm_to_watt(double value_in_dBm)
{
	double value_in_Watt = 0.001 * pow(10, 0.1 * value_in_dBm);
	return value_in_Watt;
}

/* Unit Conversion for convenience: watt to dBm */
double MainWindow::convert_watt_to_dbm(double value_in_watt)
{
	double value_in_dBm = (10 * log10(value_in_watt)) + 30;
	return value_in_dBm;
}

/*
***************************************************************************************************************************************
* Serial Port Management
***************************************************************************************************************************************
*/

/* Error Handler for the SerialPort
 * Ignore NoError state or TimeoutError state, as sweep can sometimes take a bit of time to complete.
 * For other errors just pop up an informative message box and close the serialport */
void MainWindow::serialport_error_handler(QSerialPort::SerialPortError error)
{
	if (error == QSerialPort::NoError || error == QSerialPort::TimeoutError)
	{
		return;
	}
	else
	{
		QMessageBox message;
		message.critical(0,	"Serialport Error", SG_port->errorString());
		on_pushButton_disconnect_clicked();
	}
	qDebug() << error;
}

/* Check available serial ports, if the port contents have changed (either the count or existing names) update the combobox.
 * A timer (port_poll_timer) calls this function every second. */
void MainWindow::update_port_list()
{
	QList<QSerialPortInfo> port_info = QSerialPortInfo::availablePorts();
	bool ports_changed = false;

	if (port_info_old.count() != port_info.count())
	{
		ports_changed = true;
	}
	else
	{
		for (int i = 0; i < port_info_old.count(); i++)
		{
			if (port_info_old.at(i).portName() != port_info.at(i).portName())
			{
				ports_changed = true;
			}
		}
	}

	if (ports_changed)
	{
		ui->comboBox_ports->clear();
		for (int i = 0; i < port_info.count(); i++)
		{
			ui->comboBox_ports->addItem(port_info.at(i).portName());
		}
	}

	port_info_old = port_info;
}

/* Populate and return a list of serialports that can be identified as a signal generator board according to their vendor and product ID */
QList<QSerialPortInfo> MainWindow::autodetect_SG_port()
{
	QList<QSerialPortInfo> infolist;
	for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts())
	{
		if ((info.vendorIdentifier() == 8137 && info.productIdentifier() == 131))
		{
			infolist += info;
		}
	}

	if (infolist.size() > 1)
	{
		qDebug() << "Multiple signal generator boards found";
	}

	/* If the list comes up empty show a pop-up message and exit the program
	 * This is done for the sake of simplicity, as the function that requests the list would crash the application were it to return an empty list. */
	if (infolist.isEmpty())
	{
		QMessageBox message;
		message.critical(0,	"COULD NOT AUTO-DETECT SIGNAL GENERATOR BOARD",
							"No signal generator board auto-detected at any port."
							"\nSerial connection cannot be established.\n"
							"\n1. Ensure signal generator board is connected."
							"\n2. Try again."
							"\n3. If problem persist try manual connect");
		exit(-1);
	}

	return infolist;
}

/*
***************************************************************************************************************************************
* Serial Message handling
* Functions for sending and receiving transmissions over the serial port interface.
***************************************************************************************************************************************
*/

/* WriteRead function for common commands with single line responses
 * Write the outbound string to the serialport, wait for a response that ends with \r\n */
QString MainWindow::writeRead(QString tx)
{
	if (!SG_port->isOpen())
	{
		return "";
	}

	QString rx = "";

	/* write message to serialport */
	qDebug() << "TX:\t" << (const char*)(tx+"\n").toUtf8();
	SG_port->write((const char*)(tx+"\r\n").toUtf8());

	if (SG_port->waitForBytesWritten(250))
	{
		print_message(direction::outbound, tx);
	}

	/* Wait until SGx returns complete answer */
	while (!rx.contains("\r\n"))
	{
		SG_port->waitForReadyRead(500);		//Wait up to 500ms until QSerialPort emits ReadyRead() signal
		rx += QString::fromUtf8(SG_port->readAll());

		if (rx.contains("ERR") || !SG_port->isOpen())
		{
			break;
		}
	}

	qDebug() << "RX:\t" << rx << endl;
	print_message(direction::inbound, rx);

	return rx;
}

/* WriteRead function for commands that return more than one line and end with OK
 * Write the outbound string to the serialport, wait for a response that ends with OK\r\n */
QString MainWindow::writeRead_OK(QString tx)
{
	if (!SG_port->isOpen())
	{
		return "";
	}

	QString rx = "";

	/* write message to serialport */
	if(SG_port->isOpen())
	{
		SG_port->write((const char*)(tx+"\r\n").toUtf8());

		if (SG_port->waitForBytesWritten(250))
		{
			qDebug() << "TX:\t" << (const char*)(tx+"\n").toUtf8();
			print_message(direction::outbound, tx);
		}
	}

	/* Wait until SGx returns complete answer */
	while (!rx.contains("OK\r\n"))
	{
		SG_port->waitForReadyRead(500);		//Wait up to 500ms until QSerialPort emits ReadyRead() signal
		rx += QString::fromUtf8(SG_port->readAll());
		if (rx.contains("ERR") || !SG_port->isOpen())
		{
			break;
		}
	}

	qDebug() << "RX:\t" << rx << endl;
	print_message(direction::inbound, rx);

	return rx;
}

/* Show inbound and outbound messages in the message log */
void MainWindow::print_message(MainWindow::direction dir, QString text)
{
	QString str = "";
	if (dir == direction::inbound)
	{
		str += ("<\t");

		//Adjusted cosmetics for multi-line responses
		if (text.count("\r\n") > 1)
		{
			text.replace("\r\n", "\r\n<\t");
			text.chop(2);	//Chop the last <\t
		}
	}
	else
	{
		str += (">\t");
	}

	ui->plainTextEdit->appendPlainText(str + text);
}

/*
***************************************************************************************************************************************
* Serial Port Management
***************************************************************************************************************************************
*/

/* Changing the selection in the ports combobox updates the SG_port portname parameter */
void MainWindow::on_comboBox_ports_activated(const QString &arg1)
{
	SG_port->setPortName(arg1);
	qDebug() << "Port name:" << SG_port->portName();
}

/* Try to open to the configured serial port, if succesful enable the main section of the GUI and let the user start sending commands */
void MainWindow::on_pushButton_connect_clicked()
{
	if (SG_port->open(QIODevice::ReadWrite))
	{
		show_connection_buttons(false);
		show_main_buttons(true);
		qDebug() << "Port Opened";
	}
}

/* Close the current serial port (unless already closed) and disable the main section of the GUI. */
void MainWindow::on_pushButton_disconnect_clicked()
{
	if (SG_port->isOpen())
	{
		SG_port->close();
		show_connection_buttons(true);
		show_main_buttons(false);
		qDebug() << "Port Closed";
	}
}

/* Let the GUI automatically search for a port with an signal generator board and connect to it */
void MainWindow::on_pushButton_autoconnect_clicked()
{
	ui->comboBox_ports->setCurrentText(autodetect_SG_port().at(0).portName());	//Select the first connection that looks like an signal generator board
	on_comboBox_ports_activated(ui->comboBox_ports->currentText());				//Set the portname to the current value
	on_pushButton_connect_clicked();											//Activate the connect function
}

/*
***************************************************************************************************************************************
* Buttons for sending commands.
* Nothing too exciting going on here, just calling the writeRead/writeRead_OK functions with the appropriate command strings.
* Some commands will use an input from GUI
***************************************************************************************************************************************
*/

/* Send Commands */
void MainWindow::on_pushButton_get_identity_clicked()
{
	writeRead("$IDN,0");
}

void MainWindow::on_pushButton_get_version_clicked()
{
	writeRead("$VER,0");
}

void MainWindow::on_pushButton_get_status_1_clicked()
{
	writeRead("$ST,0");
}

void MainWindow::on_pushButton_get_status_2_clicked()
{
	writeRead_OK("$ST,0,1");
}

void MainWindow::on_pushButton_clear_errors_clicked()
{
	writeRead("$ERRC,0");
}

void MainWindow::on_pushButton_get_frequency_clicked()
{
	writeRead("$FCG,0");
}

void MainWindow::on_pushButton_set_frequency_clicked()
{
	writeRead("$FCS,0," + ui->lineEdit_frequency->text());
}

void MainWindow::on_pushButton_get_PA_power_measurement_clicked()
{
	writeRead("$PPG,0");
}

void MainWindow::on_pushButton_get_power_clicked()
{
	writeRead("$PWRG,0");
}

void MainWindow::on_pushButton_set_power_clicked()
{
	writeRead("$PWRS,0," + ui->lineEdit_power->text());
}

void MainWindow::on_pushButton_set_DLL_clicked()
{
	writeRead("$DLCS,0," + ui->lineEdit_DLL_1->text() + "," + ui->lineEdit_DLL_2->text() + "," + ui->lineEdit_DLL_3->text() + "," + ui->lineEdit_DLL_4->text() + "," + ui->lineEdit_DLL_5->text() + "," + ui->lineEdit_DLL_6->text());
}

void MainWindow::on_pushButton_DLL_on_clicked()
{
	writeRead("$DLES,0,1");
}

void MainWindow::on_pushButton_DLL_off_clicked()
{
	writeRead("$DLES,0,0");
}

void MainWindow::on_pushButton_RF_on_clicked()
{
	writeRead("$ECS,0,1");
}

void MainWindow::on_pushButton_RF_off_clicked()
{
	writeRead("$ECS,0,0");
}

/*
***************************************************************************************************************************************
* Sweeping
* Probably the most interesting bit of the code, as it involves parsing data returned by the signal generator.
***************************************************************************************************************************************
*/

/* Power input of the sweep is available in watt or dBm at the user's convenience
 * Changing the one unit, automatically adjusts the other */
void MainWindow::on_lineEdit_SWP_4_textEdited(const QString &arg1)
{
	ui->lineEdit_SWP_5->setText(QString::number(convert_dbm_to_watt(arg1.toDouble())));
}

void MainWindow::on_lineEdit_SWP_5_textEdited(const QString &arg1)
{
	ui->lineEdit_SWP_4->setText(QString::number(convert_watt_to_dbm(arg1.toDouble())));
}

/* Buttons for changing the sweep plot notation form
 * Will redraw the plot (if there is one) in the appropriate notation format */
void MainWindow::on_pushButton_SWP_notation_linear_clicked()
{
	SWP_draw_plot(S11_notation::linear);
}

void MainWindow::on_pushButton_sweep_notation_logarithmic_clicked()
{
	SWP_draw_plot(S11_notation::logarithmic);
}

/* Execute an S11 Sweep */
void MainWindow::on_pushButton_execute_sweep_clicked()
{
	if (SWP_run_sweep() == true)	//Only draw a plot if sweep is successful
	{
		SWP_draw_plot(S11_notation::logarithmic);
	}
}

/* Execute an S11 Sweep in dBm
 * Process the data and store it in QVectors, to be used for drawing the graph later. */
bool MainWindow::SWP_run_sweep()
{
	QString SWP_raw_data = 	writeRead_OK("$SWPD,0," + ui->lineEdit_SWP_1->text()
											  + "," + ui->lineEdit_SWP_2->text()
											  + "," + ui->lineEdit_SWP_3->text()
											  + "," + ui->lineEdit_SWP_4->text()
											  + ",0");
	QStringList SWP_data;

	/* If the data checks out, split the long message up by the individual lines */
	if(SWP_raw_data.contains("$SWPD,") && SWP_raw_data.contains("OK\r\n"))
	{
		SWP_data = SWP_raw_data.split("\r\n");
		//Remove the OK entry + the empty line that comes after.
		SWP_data.removeLast();
		SWP_data.removeLast();
	}
	else
	{
		QMessageBox message;
		message.warning(0, "Sweep Error", "Sweep data invalid / incomplete.");
		return false;
	}

	/* Resize the QVectors for the correct amount of data */
	SWP_freq_data.resize(SWP_data.count());
	SWP_fwd_data.resize(SWP_data.count());
	SWP_rfl_data.resize(SWP_data.count());
	SWP_s11_dbm_data.resize(SWP_data.count());
	SWP_s11_watt_data.resize(SWP_data.count());

	/* Process the data */
	for (int i = 0; i < SWP_data.count(); i++)
	{
		/* Split each line of SWP data by the comma's */
		QStringList data = SWP_data.at(i).split(",");

		/* Fill the QVectors with data, calculate S11 */
		if (data.contains("$SWPD") && data.count() == 5)
		{
			SWP_freq_data[i] = data.at(2).toDouble();
			SWP_fwd_data[i] = data.at(3).toDouble();
			SWP_rfl_data[i] = data.at(4).toDouble();

			/* Calculate S11 (dB) / Reflection (%) using the above forward and reflected powers */
			SWP_s11_dbm_data[i] = SWP_rfl_data[i] - SWP_fwd_data[i];														//S11 (dB)
			SWP_s11_watt_data[i] = (convert_dbm_to_watt(SWP_rfl_data[i]) / convert_dbm_to_watt(SWP_fwd_data[i])) * 100;		//Reflection (%)
		}
	}
	return true;
}

/* Draw the an S11 graph based on the measured data in the correct notation format.
 * Uses the QCustomPlot library. See QCustomPlot documentation: https://www.qcustomplot.com/documentation/index.html  */
void MainWindow::SWP_draw_plot(S11_notation notation)
{
	if (SWP_freq_data.isEmpty() || SWP_s11_dbm_data.isEmpty() || SWP_s11_watt_data.isEmpty())
	{
		return;
	}

	ui->SWP_plot->addGraph();
	ui->SWP_plot->xAxis->setLabel("Frequency (MHz)");
	ui->SWP_plot->xAxis->setRange(SWP_freq_data[0],SWP_freq_data[SWP_freq_data.size()-1]);
	ui->SWP_plot->yAxis->setNumberFormat("f");
	ui->SWP_plot->yAxis->setNumberPrecision(2);
	double min_val, max_val;

	if(notation == S11_notation::logarithmic)
	{
		ui->SWP_plot->graph(0)->setData(SWP_freq_data,SWP_s11_dbm_data);

		min_val = *std::min_element(SWP_s11_dbm_data.constBegin(),SWP_s11_dbm_data.constEnd());
		if (min_val > 0){
			 min_val = 0;
		}
		max_val = *std::max_element(SWP_s11_dbm_data.constBegin(),SWP_s11_dbm_data.constEnd());
		if(max_val < 0){
			 max_val = 0;
		}

		ui->SWP_plot->yAxis->setRange(min_val*1.1, max_val*1.1);
		ui->SWP_plot->yAxis->setLabel("S11 (dB)");
	}
	else
	{
		ui->SWP_plot->graph(0)->setData(SWP_freq_data,SWP_s11_watt_data);

		max_val = *std::max_element(SWP_s11_watt_data.constBegin(),SWP_s11_watt_data.constEnd());

		if(max_val <= 100)
		{
			max_val = 100;
		}
		ui->SWP_plot->yAxis->setRange(0,max_val);
		ui->SWP_plot->yAxis->setLabel("Reflection (%)");
	}

	ui->SWP_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectItems);
	ui->SWP_plot->replot();
}

/*
***************************************************************************************************************************************
* About button
***************************************************************************************************************************************
*/

/* About button shows a pop-up with about text */
void MainWindow::on_pushButton_about_clicked()
{
	QMessageBox message;
	QFile file(":/about.txt");
	if(!file.open(QIODevice::ReadOnly))
	{
		message.critical(0,"Error", "Could not open about text");
	}
	else
	{
		message.about(0,"About", file.readAll());
	}
	file.close();
}

