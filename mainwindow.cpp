/*
 * Environment
 * Machine          : Nvidia Jetson nano
 * Micorcontroller  : Arduino nano
 * Sensors          : Illuminance sensor
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <QIntValidator>
#include <QDebug>
#include <QThread>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define GPIO_DIR "/sys/class/gpio"

using namespace std;
using namespace cv;

static int value = 0;
static int t_prev;
static int t_prev2;
static int t_cur;
static int t_cur2;
static double interval;
static double interval2;
int cnt = 0;

int i;
int led_gp = 216;
char buf[256];
FILE *led_gpio;

Mat dummy_1ch = Mat::zeros(Size(800, 600), CV_8UC1);
Mat dummy_3ch = Mat::zeros(Size(800, 600), CV_8UC3);
QImage dummy_img_1ch(dummy_1ch.data, dummy_1ch.cols, dummy_1ch.rows, QImage::Format_Grayscale8);
QImage dummy_img_3ch(dummy_3ch.data, dummy_3ch.cols, dummy_3ch.rows, QImage::Format_RGB888);

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    serial = new QSerialPort();
    serial->setPortName("/dev/ttyUSB0");
    serial->setBaudRate(QSerialPort::Baud115200);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if(!serial->open(QIODevice::ReadWrite)){
        qDebug() << "\n Serial port open error \n";
    }

    char tmp_path[] = {"/sys/class/gpio/gpio216/"};
    int result = access(tmp_path, 0);

    if (result==-1){
        snprintf(buf, sizeof(buf), "%s/export", GPIO_DIR);
        led_gpio = fopen(buf, "w");
        fprintf(led_gpio, "%d\n", led_gp);
        fclose(led_gpio);
        qDebug() << "chk1";
    }

    snprintf(buf, sizeof(buf), "%s/gpio%d/direction", GPIO_DIR, led_gp);
    led_gpio = fopen(buf, "w");
    fprintf(led_gpio, "out\n");
    fclose(led_gpio);
    qDebug() << "chk2";

    snprintf(buf, sizeof(buf), "%s/gpio%d/value", GPIO_DIR, led_gp);
    led_gpio = fopen(buf, "w");
    fprintf(led_gpio, "%d\n", 0);
    fclose(led_gpio);
    qDebug() << "chk3";

    QIntValidator *portValidator = new QIntValidator(0, 255);
    QIntValidator *strideValidator = new QIntValidator(1, 50);

    ui->camPort->setValidator(portValidator);
    ui->dialDegree->setNotchesVisible(true);
    ui->strideEdit->setValidator(strideValidator);

    ui->SRC->setPixmap(QPixmap::fromImage(dummy_img_3ch).scaled(ui->SRC->width(), ui->SRC->height(), Qt::KeepAspectRatio));
    ui->ROI->setPixmap(QPixmap::fromImage(dummy_img_3ch).scaled(ui->ROI->width(), ui->ROI->height(), Qt::KeepAspectRatio));
    ui->imageEnhenced->setPixmap(QPixmap::fromImage(dummy_img_3ch).scaled(ui->imageEnhenced->width(), ui->imageEnhenced->height(), Qt::KeepAspectRatio));
    ui->imageR->setPixmap(QPixmap::fromImage(dummy_img_3ch).scaled(ui->imageR->width(), ui->imageR->height(), Qt::KeepAspectRatio));
    ui->imageG->setPixmap(QPixmap::fromImage(dummy_img_3ch).scaled(ui->imageG->width(), ui->imageG->height(), Qt::KeepAspectRatio));
    ui->imageB->setPixmap(QPixmap::fromImage(dummy_img_3ch).scaled(ui->imageB->width(), ui->imageB->height(), Qt::KeepAspectRatio));

    connect(ui->PBTrecord, SIGNAL(clicked()), this, SLOT(record()));

    connect(ui->PBTrun, SIGNAL(clicked()), this, SLOT(runCamera()));
    connect(ui->dialDegree, &QDial::valueChanged, this, &MainWindow::dialValueChanged);
    connect(ui->spinDegree, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::spinValueChanged);
    connect(ui->PBTup, SIGNAL(clicked()), this, SLOT(pbtUpClicked()));
    connect(ui->PBTdown, SIGNAL(clicked()), this, SLOT(pbtDownClicked()));
    connect(ui->PBTleft, SIGNAL(clicked()), this, SLOT(pbtLeftClicked()));
    connect(ui->PBTright, SIGNAL(clicked()), this, SLOT(pbtRightClicked()));
    connect(ui->PBTplus, SIGNAL(clicked()), this, SLOT(pbtPlusClicked()));
    connect(ui->PBTminus, SIGNAL(clicked()), this, SLOT(pbtMinusClicked()));
    connect(ui->PBTstride, SIGNAL(clicked()), this, SLOT(pbtStrideClicked()));
    connect(serial, SIGNAL(readyRead()), this, SLOT(receive()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::runCamera(void){

    if(this->RUNNING==true){
        // Illuminance thread stop

        ui->SRC->setPixmap(QPixmap::fromImage(dummy_img_3ch).scaled(ui->SRC->width(), ui->SRC->height(), Qt::KeepAspectRatio));
        ui->ROI->setPixmap(QPixmap::fromImage(dummy_img_3ch).scaled(ui->ROI->width(), ui->ROI->height(), Qt::KeepAspectRatio));
        ui->imageEnhenced->setPixmap(QPixmap::fromImage(dummy_img_3ch).scaled(ui->imageEnhenced->width(), ui->imageEnhenced->height(), Qt::KeepAspectRatio));
        ui->imageR->setPixmap(QPixmap::fromImage(dummy_img_3ch).scaled(ui->imageR->width(), ui->imageR->height(), Qt::KeepAspectRatio));
        ui->imageG->setPixmap(QPixmap::fromImage(dummy_img_3ch).scaled(ui->imageG->width(), ui->imageG->height(), Qt::KeepAspectRatio));
        ui->imageB->setPixmap(QPixmap::fromImage(dummy_img_3ch).scaled(ui->imageB->width(), ui->imageB->height(), Qt::KeepAspectRatio));
        ui->PBTrun->setText("Run");
        this->RUNNING = false;
        return;

    }else if(this->RUNNING==false){

        int port = ui->camPort->text().toInt();

        VideoCapture cap(port);

        if(!cap.isOpened()){
            QMessageBox::critical(this, "ERROR", "Please check your camera and port number.", QMessageBox::Ok);
            return;
        }
        // Illuminance thread start

        this->camPort = port;
        ui->PBTrun->setText("Stop");

        this->RUNNING = true;

        Mat frame, gray, enhenced;
        Mat img_empty, img_red, img_green, img_blue;

        QImage src, roi, imgR, imgG, imgB, ENHEN;

        Mat channels[3];
        Mat ROI, ROI_;

        int illuminance = this->ILLUMINANCE;
        this->COMPENSATION = (int)((this->ILLUMINANCE - this->ILL_MIN)*255/1024 * 0.7);

        double fps = cap.get(CAP_PROP_FPS);
        Size frameSize = Size(640, 480);
        int fourcc = VideoWriter::fourcc('X','V','I','D');
        vector<Mat> video;
        t_prev2 = clock(); // illuminance sensor
        while(RUNNING==true){
            cap >> frame;
            cv::resize(frame, frame, frameSize);
            cvtColor(frame, frame, COLOR_BGR2RGB);

            src = QImage(frame.data, frame.cols, frame.rows, QImage::Format_RGB888);
            ui->SRC->setPixmap(QPixmap::fromImage(src).scaled(ui->SRC->width(), ui->SRC->height(), Qt::KeepAspectRatio));

            t_cur2 = clock();
            interval2 = (float)((t_cur2 - t_prev2)/CLOCKS_PER_SEC);
            if(interval2 >= 0.1){
                illuminance = this->ILLUMINANCE;
                this->COMPENSATION = (int)((this->ILLUMINANCE - this->ILL_MIN)*255/1024*0.7);
                t_prev2 = t_cur2;
            }
            cvtColor(frame, gray, COLOR_RGB2GRAY);
            qDebug() << " Compenstaion : " << this->COMPENSATION;
            enhenced = gray + this->COMPENSATION;
            ENHEN = QImage(enhenced.data, enhenced.cols, enhenced.rows, QImage::Format_Grayscale8);
            ui->imageEnhenced->setPixmap(QPixmap::fromImage(ENHEN).scaled(ui->imageEnhenced->width(), ui->imageEnhenced->height(), Qt::KeepAspectRatio));

            // Recording video
            if(this->RECORDING_START){

                if(this->RECORDING_END==false){
                    if(cnt == 0){
                        cnt++;
                        t_prev = clock();
                        value=1;
                    }
                    t_cur = clock();
                    interval = (double)((t_cur - t_prev) / CLOCKS_PER_SEC);
                    qDebug() << "chk_t: " << cnt << ", " << t_prev << ", " << t_cur << ", " << interval;
                    Mat tmp;
                    frame.copyTo(tmp);
                    video.push_back(tmp);
                    if(interval >= 0.3){
                        value -= 1;
                        value *= -1;
                        t_prev = clock();
                    }
                    snprintf(buf, sizeof(buf), "%s/gpio%d/value", GPIO_DIR, led_gp);
                    led_gpio = fopen(buf, "w");
                    fprintf(led_gpio, "%d\n", value);
                    fclose(led_gpio);

                    qDebug() << "chk_LED: " << value;

                }else{
                    string filename = format("out%d.avi", this->VID_COUNT);
                    VideoWriter outputVideo(filename, fourcc, fps, frameSize, true);

                    for(size_t i = 0; i < video.size(); i++){
                        outputVideo.write(video.at(i));
                    }
                    outputVideo.release();
                    vector<Mat> video;

                    this->VID_COUNT++;
                    this->RECORDING_START = false;
                    this->RECORDING_END = false;

                    value = 0;
                    cnt=0;

                    snprintf(buf, sizeof(buf), "%s/gpio%d/value", GPIO_DIR, led_gp);
                    led_gpio = fopen(buf, "w");
                    fprintf(led_gpio, "%d\n", 0);
                    fclose(led_gpio);
                    /*
                    snprintf(buf, sizeof(buf), "%s/unexport", GPIO_DIR);
                    led_gpio = fopen(buf, "w");
                    fprintf(led_gpio, "%d\n", led_gp);
                    fclose(led_gpio);
                    */
                }
            }

            img_empty = Mat::zeros(Size(frame.cols, frame.rows), CV_8UC1);

            //cvtColor(frame, frame, COLOR_BGR2RGB);
            split(frame, channels); // R, G, B

            this->width = frame.cols;
            this->height = frame.rows;
            this->CUR_X_RANGE_MAX = width - 100/this->MAG_CUR;
            this->CUR_Y_RANGE_MAX = height - 100/this->MAG_CUR;

            // ROI
            int original_width = frame.cols;
            int original_height = frame.rows;
            int ROI_x_left, ROI_x_right, ROI_y_top, ROI_y_bottom;
            int ROI_width = 100 / this->MAG_CUR;
            this->ROI_WIDTH = ROI_width;

            ROI_x_left = this->ROI_LEFT_TOP.y;
            ROI_y_top = this->ROI_LEFT_TOP.x;
            ROI_x_right = ROI_x_left + ROI_width;
            ROI_y_bottom = ROI_y_top + ROI_width;

            this->CUR_X_RANGE_MAX = original_width - ROI_width;
            this->CUR_Y_RANGE_MAX = original_height - ROI_width;

            ROI = frame(Range(ROI_x_left, ROI_x_right), Range(ROI_y_top, ROI_y_bottom));
            //qDebug() << this->MAG_CUR << ": " << ROI_width << ", " << ROI_x_left << ", " << ROI_x_right << ", " << ROI_y_top << ", " << ROI_y_bottom << endl;

            cv::resize(ROI, ROI_, Size(100, 100));

            // ROI rotation
            Mat rotation_matrix = getRotationMatrix2D(Point2f(ROI_.cols/2, ROI_.rows/2), (float)this->ROT_DEGREE, 1.0);
            warpAffine(ROI_, ROI_, rotation_matrix, Size(100, 100));

            roi = QImage(ROI_.data, ROI_.cols, ROI_.rows, QImage::Format_RGB888);
            ui->ROI->setPixmap(QPixmap::fromImage(roi).scaled(ui->ROI->width(), ui->ROI->height(), Qt::KeepAspectRatio));

            // R-channel
            vector<Mat> mergedR;
            mergedR.push_back(channels[0]);
            mergedR.push_back(img_empty);
            mergedR.push_back(img_empty);
            merge(mergedR, img_red);
            imgR = QImage(img_red.data, img_red.cols, img_red.rows, QImage::Format_RGB888);
            ui->imageR->setPixmap(QPixmap::fromImage(imgR).scaled(ui->imageR->width(), ui->imageR->height(), Qt::KeepAspectRatio));

            // G-channel
            vector<Mat> mergedG;
            mergedG.push_back(img_empty);
            mergedG.push_back(channels[1]);
            mergedG.push_back(img_empty);
            merge(mergedG, img_green);
            imgG = QImage(img_green.data, img_green.cols, img_green.rows, QImage::Format_RGB888);
            ui->imageG->setPixmap(QPixmap::fromImage(imgG).scaled(ui->imageG->width(), ui->imageG->height(), Qt::KeepAspectRatio));

            // B-channel
            vector<Mat> mergedB;
            mergedB.push_back(img_empty);
            mergedB.push_back(img_empty);
            mergedB.push_back(channels[2]);
            merge(mergedB, img_blue);
            imgB = QImage(img_blue.data, img_blue.cols, img_blue.rows, QImage::Format_RGB888);
            ui->imageB->setPixmap(QPixmap::fromImage(imgB).scaled(ui->imageB->width(), ui->imageB->height(), Qt::KeepAspectRatio));

            waitKey(1);
        }
        cap.release();
        ui->PBTrun->setText("Stop");
    }
}

void MainWindow::pbtStrideClicked(void){
    this->STRIDE = ui->strideEdit->text().toInt();
}

void MainWindow::dialValueChanged(void){
    int val = ui->dialDegree->value();
    this->ROT_DEGREE = val;
    ui->spinDegree->setValue(val);
}

void MainWindow::spinValueChanged(void){
    int val = ui->spinDegree->value();
    this->ROT_DEGREE = val;
    ui->dialDegree->setValue(val);
}

void MainWindow::pbtUpClicked(void){
    int x = this->ROI_LEFT_TOP.x;
    int y = this->ROI_LEFT_TOP.y;

    if(y-this->STRIDE < 0){
       return;
    }else{
        y-=this->STRIDE;
    }
    this->ROI_LEFT_TOP = Point(x, y);
}

void MainWindow::pbtDownClicked(void){
    int x = this->ROI_LEFT_TOP.x;
    int y = this->ROI_LEFT_TOP.y;

    if(y+this->STRIDE > this->height-this->ROI_WIDTH-1){
        return;
    }else{
        y+=this->STRIDE;
    }
    this->ROI_LEFT_TOP = Point(x, y);
}

void MainWindow::pbtLeftClicked(void){
    int x = this->ROI_LEFT_TOP.x;
    int y = this->ROI_LEFT_TOP.y;

    if(x-this->STRIDE < 0){
        return;
    }else{
        x-=this->STRIDE;
    }
    this->ROI_LEFT_TOP = Point(x, y);
}

void MainWindow::pbtRightClicked(void){
    int x = this->ROI_LEFT_TOP.x;
    int y = this->ROI_LEFT_TOP.y;

    if(x+this->STRIDE > this->width-this->ROI_WIDTH-1){
        return;
    }else{
        x+=STRIDE;
    }
    this->ROI_LEFT_TOP = Point(x, y);
}

void MainWindow::pbtPlusClicked(void){
    if(this->MAG_CUR != this->MAG_MAX) this->MAG_CUR++;

}

void MainWindow::pbtMinusClicked(void){
    if(this->MAG_CUR != this->MAG_MIN) this->MAG_CUR--;
}

void MainWindow::record(void){
    if(this->RECORDING_START){
        ui->PBTrecord->setText("RECORD");
        this->RECORDING_END=true;

    }else{
        ui->PBTrecord->setText("STOP");
        this->RECORDING_START=true;

    }
}

void MainWindow::receive(){
    QByteArray read_data;
    read_data = serial->readAll();
    this->ILLUMINANCE = read_data.toInt();

    qDebug() << "chk";
}
