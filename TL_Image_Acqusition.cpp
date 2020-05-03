/* Filename: TL_Image_Acqusition.cpp
   Auther: Sankalp Pund
   Course: RTES Spring 2020
   Description: This file has code for Time Lapse Image Acquisition project. 
*/

/*Header files*/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <time.h>
#include <sys/param.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <mqueue.h>
#include <stdbool.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <sys/utsname.h>


using namespace cv;
using namespace std;

VideoCapture cap(0);

Mat mat_ppm(480,640,CV_8UC3);
Mat mat_jpg(480,640,CV_8UC3);
uint8_t *frame_ptr;

/*Macros*/
#define HRES 640
#define VRES 480
#define MSEC 1000000
#define NSEC_PER_SEC (1000000000)
#define No_Of_Frames  2000
#define SERVICES 4
#define OK (0)
#define TRUE (1)
#define FALSE (0)
#define ERROR (-1)
#define RATE 30

/*Global Variables*/
double time_gap=0;
int device = 0;
time_t rawtime;
struct tm * timecur;
sem_t semaphore[SERVICES];
pthread_t Threads[SERVICES];
pthread_attr_t attribute_object[SERVICES];
struct sched_param priority_set[SERVICES];
void* (*TL_Functions[SERVICES]) (void*) ;
double buff_begin[SERVICES] = {0,0,0,0};
double buff_end[SERVICES] = {0,0,0,0};
double jitter_ac_buff[SERVICES] = {0,0,0,0};
double Jitter_average[SERVICES] = {0,0,0,0};
double WCET_buff[SERVICES] = {0,0,0,0};
double average_d_buff[SERVICES] = {0,0,0,0};
uint32_t counter_arr[SERVICES] = {0,0,0,0};
double jitter_calc_arr[SERVICES] = {0,0,0,0};
double run_time[SERVICES] = {0,0,0,0};
static struct timespec start_time, stop_time, execution, current_time;
static uint32_t ctr = 0, ctr2=0;
static uint8_t bool_flag = TRUE;
static struct timespec Frame_capture_begin = {0,0};
static struct timespec Frame_capture_stop = {0,0};
static struct mq_attr frame_mq_attr;
double initial_time;
sem_t semaphore_for_ppm, semaphore_for_jpg,semaphore_ppm_done, semaphore_timestamp, semaphore_for_time;
void display(int disp, uint8_t w, uint8_t* store);

double milliseconds_time(void);

/*Function Name: Jitter()
  Description: This function calculates jitter and WCET
  input: uint8_t index parameter
  return : none
*/
void Jitter(uint8_t T_index)
{
	
    printf("\n\rstart time(ms): %0.8lf", buff_begin[T_index]);

    buff_end[T_index] = milliseconds_time();

    printf("\n\rstop time (ms): %0.8lf", buff_end[T_index]);

    run_time[T_index] = buff_end[T_index] - buff_begin[T_index];

	if(run_time[T_index] > WCET_buff[T_index])
	{
		WCET_buff[T_index] = run_time[T_index]; 

		cout<<"\n\rWCET(ms) for Thread:"<<T_index+0<<" ="<<WCET_buff[T_index]/100;
	}
	if(counter_arr[T_index] == 0)
	{
      	average_d_buff[T_index] = run_time[T_index]; 
    }
	else if(counter_arr[T_index] > 0)
	{
		jitter_calc_arr[T_index] = run_time[T_index] - average_d_buff[T_index];

		average_d_buff[T_index] = (average_d_buff[T_index] * (counter_arr[T_index]-1) + run_time[T_index])/(counter_arr[T_index]);
  	   		
		printf("\rJitter (ms) : %0.8lf", jitter_calc_arr[T_index]);

		jitter_ac_buff[T_index] += jitter_calc_arr[T_index];
	}
	counter_arr[T_index]++;
}

/*Function Name: Jitter_val()
  Description: This function prints jitter and WCET
  input: uint8_t index parameter
  return : none
*/
void Jitter_val(uint8_t T_index)
{
	
	cout<<"\n\rAccumulated Jitter for Thread = "<<T_index+0<<" ="<<jitter_ac_buff[T_index]; 
	cout<<"\n\rWCET(ms) for Thread : "<<T_index+0<<" ="<<WCET_buff[T_index]/100;
	Jitter_average[T_index]=jitter_ac_buff[T_index]/No_Of_Frames;  
	cout<<"\n\rAverage Jitter for thread = "<<T_index+0<<" ="<<Jitter_average[T_index]; 

}

/*Function Name: milliseconds_time()
  Description: This function calculates the value in milliseconds
  input: none
  return : none
*/
double milliseconds_time(void)
{
	struct timespec scene = {0,0};
	clock_gettime(CLOCK_REALTIME, &scene);
	return ((scene.tv_sec*1000)+scene.tv_nsec/MSEC);
}


/*Function Name: thread_init()
  Description: This function initializes semaphores and create threads.
  input: none
  return : void
*/

void threads_init(void)
{
	cout<<"Initializing thread creation."<<endl;
	
	uint8_t i=0;

	uint8_t rt_highest_Priority = sched_get_priority_max(SCHED_FIFO); 


	for(i=0;i<SERVICES;i++)
	{	
		//This function initializes the thread attributes
        //object pointed to by attribute_object with default attribute values.
		pthread_attr_init(&attribute_object[i]);

		//In this function the Threads that are created using attribute_object take their scheduling
        //attributes from the values specified by the attributes object.
		pthread_attr_setinheritsched(&attribute_object[i],PTHREAD_EXPLICIT_SCHED);

		//In this function,  The pthread_attr_setschedpolicy() function sets the scheduling policy
        //attribute of the thread attributes object referred to by attribute_object to the
        //value specified in policy.  This attribute determines the scheduling
        //policy of a thread. Policy here is SCHED_FIFO.
		pthread_attr_setschedpolicy(&attribute_object[i],SCHED_FIFO);

		priority_set[i].sched_priority=rt_highest_Priority-i;
		
				
		if (sem_init (&semaphore[i], 0, 0))
		{
			cout<<"\n\rSemaphore Initialization Unsuccessful"<<i;
			exit (-1);
		}
		if(sem_init(&semaphore_for_ppm, 0,1))
		{
			cout<<"\n\rSemaphore_for_ppm Initialization Unsuccessful";
			exit (-1);
		}
		if(sem_init(&semaphore_ppm_done, 0,0))
		{
			cout<<"\n\rsemaphore_ppm_done Initialization Unsuccessful";
			exit (-1);
		}
		if(sem_init(&semaphore_timestamp, 0,1))
		{
			cout<<"\n\rsemaphore_timestamp Initialization Unsuccessful";
			exit (-1);
		}
		if(sem_init(&semaphore_for_time, 0,0))
		{
			cout<<"\n\rsemaphore_for_time Initialization Unsuccessful";
			exit (-1);
		}

		//This function sets the scheduling
        //parameter attributes of the thread attributes object referred to by
        //attribute_object to the values specified in the buffer pointed to by priority_set.
        pthread_attr_setschedparam(&attribute_object[i], &priority_set[i]);

        //This function starts a new thread in the calling process.  
		if(pthread_create(&Threads[i], &attribute_object[i], TL_Functions[i], (void *)0) !=0)
		{
			perror("pthread create unsuccessful");
			exit(-1);
		}

	}
	
	//This function increments (unlocks) the semaphore pointed to by semaphore[1].
	sem_post(&semaphore[1]);

	//This function waits for the thread specified by Threads[i] to terminate.
	for(i=0;i<SERVICES;i++)
	{
  		pthread_join(Threads[i],NULL);
	}
}


/*Function Name: counter()
  Description: This function sets the flag once all frames captured.
  input: none
  return : void
*/
void counter(void)
{
	if(ctr==No_Of_Frames)
	{
		bool_flag = FALSE;
	}

	ctr++;
}


/*Function Name: sequencer()
  Description: This function is a main scheduler.
  input: pointer to void.
  return : void
  Ref: This function is developed upon sequncer provided by professor sam siewarts.
*/

void *sequencer(void *thread_index)
{
	uint8_t T_index = 0,i=0;	
	struct timespec delay_time = {0, 33333333};
	struct timespec remaining_time;
	uint32_t time_old = 1;
	unsigned long long seqCnt =0; 
	double residual; int rc; int delay_cnt =0;
	clock_gettime(CLOCK_REALTIME, &current_time);
	printf("\n\rThe current time is %d sec and %d nsec\n", current_time.tv_sec, current_time.tv_nsec);
	do 
	{       
		delay_cnt=0; 
		residual=0.0;

		clock_gettime(CLOCK_REALTIME, &current_time);

			do
			{
				rc=nanosleep(&delay_time, &remaining_time);

				if(rc==EINTR)
				{
					residual = remaining_time.tv_sec + ((double)remaining_time.tv_nsec/(double) NSEC_PER_SEC);

					delay_cnt++;
				}
				else if(rc < 0)
				{
					perror(" Sequencer nanosleep");

					exit(-1);
				}
			}while((residual > 0.0) && (delay_cnt < 100));

			seqCnt++;

			clock_gettime(CLOCK_REALTIME, &current_time);

			if((seqCnt % RATE) == 0)
			{
				counter();

				sem_post(&semaphore[1]);
				
			}
			if((seqCnt % RATE) == 0)
			{
				
				sem_post(&semaphore[2]);
				
			}
		  	if((seqCnt % RATE) == 0)
			{
				
				sem_post(&semaphore[3]);
				
			}


		sem_post(&semaphore[0]);

	}while(bool_flag);

	
	sem_post(&semaphore[1]);
	sem_post(&semaphore[2]);
	sem_post(&semaphore[3]);

	Jitter_val(T_index);

	pthread_exit(NULL);
}


/*Function Name: capture_frames_ppm()
  Description: This function is thread to capture frames.
  input: pointer to void.
  return : void.
*/

void *capture_frames_ppm(void *thread_index)
{
	//cout<<"Executing Thread-2 of Capturing Image"<<endl;
  	
	uint8_t T_index=1;	
	
	system("uname -a > system.out");
	
 	while(bool_flag)
  	{
    	//cout<<"Waiting for Thread-1 (sequencer) to release the semaphore for Thread-2 of capturing Image";
		
		sem_wait(&semaphore[T_index]);

		//cout<<"Thread-1 (sequencer) has released the semaphore for Thread-2 which captures Image";

	    buff_begin[T_index] = milliseconds_time();

		clock_gettime(CLOCK_REALTIME, &Frame_capture_begin);

		sem_wait(&semaphore_for_ppm);

		//Stream operator to read the next frame.
		//Ref:https://docs.opencv.org/3.4/d8/dfe/classcv_1_1VideoCapture.html#a199844fb74226a28b3ce3a39d1ff6765

		cap >> mat_ppm; 

		sem_post(&semaphore_for_ppm);

		clock_gettime(CLOCK_REALTIME, &Frame_capture_stop);

		//Calculting Time gap between frame capture.
		time_gap = ((Frame_capture_stop.tv_sec - Frame_capture_begin.tv_sec)*1000000000 + (Frame_capture_stop.tv_nsec - Frame_capture_begin.tv_nsec));

		cout<<"\n\rTime required to capture a frame = "<<time_gap<<" nanoseconds";

		Jitter(T_index);
		
	}

	Jitter_val(T_index);

	pthread_exit(NULL);
}


/*Function Name: save_encode_frame()
  Description: This function is thread to save image and encode with timestamps.
  input: pointer to void.
  return : void.
*/
void *save_encode_frame(void *thread_index)
{
	
	uint8_t T_index=2;

	struct utsname sysData;

	if (uname(&sysData) != 0) 
    {
      perror("uname");
	}


	//ostringstream - write formatted text into a char array in memory.
	ostringstream name;

	//Setting up custom compression parameters.
	//Ref: https://docs.opencv.org/master/d4/da8/group__imgcodecs.html
	vector<int> compression_params;

	compression_params.push_back(CV_IMWRITE_PXM_BINARY);

	compression_params.push_back(95);

 	while(bool_flag)
	{

		sem_wait(&semaphore[T_index]);

	
		sem_wait(&semaphore_for_ppm);

    	uint8_t str[10];

		display(counter_arr[T_index],4, str); 

	    buff_begin[T_index] = milliseconds_time();

		name.str("Image_No_");

		name<<"Image_No_"<<str<<".ppm";

		time (&rawtime);

 		timecur = localtime (&rawtime);

		//Draws a user, machine information string on the frame 
		//Ref: https://docs.opencv.org/2.4/modules/core/doc/drawing_functions.html

		putText(mat_ppm,asctime(timecur),Point(100,100),FONT_HERSHEY_COMPLEX_SMALL,0.7,Scalar(255,255,0),2);

		putText(mat_ppm,sysData.sysname,Point(2, 30),FONT_HERSHEY_COMPLEX_SMALL,0.7,Scalar(255,255,0),2);

		putText(mat_ppm,sysData.nodename,Point(60, 30),FONT_HERSHEY_COMPLEX_SMALL,0.7,Scalar(255,255,0),2);

		putText(mat_ppm,sysData.release,Point(2, 45),FONT_HERSHEY_COMPLEX_SMALL,0.7,Scalar(255,255,0),2);

		putText(mat_ppm,sysData.version,Point(170, 45),FONT_HERSHEY_COMPLEX_SMALL,0.7,Scalar(255,255,0),2);

		putText(mat_ppm,sysData.machine,Point(2, 60),FONT_HERSHEY_COMPLEX_SMALL,0.7,Scalar(255,255,0),2);

		//This function saves file.
		//Ref: https://docs.opencv.org/2.4/modules/highgui/doc/reading_and_writing_images_and_video.html#imwrite
		imwrite(name.str(), mat_ppm, compression_params);

		//Loads an image from file.
		// Ref: https://docs.opencv.org/2.4/modules/highgui/doc/reading_and_writing_images_and_video.html?highlight=imread#imread
		mat_jpg = imread(name.str(),CV_LOAD_IMAGE_COLOR); 
		name.str("");

		name<<"Image_No_"<<str<<".jpg";

		//writing jpg image to string
		imwrite(name.str(), mat_jpg, compression_params); 

		name.str(" ");

		Jitter(T_index);

		//posting the semaphores.
		sem_post(&semaphore_ppm_done);

		sem_post(&semaphore_for_time);

		sem_post(&semaphore_for_ppm);
		
  	}

	Jitter_val(T_index);

	pthread_exit(NULL);
}


/*Function Name: timestamp_function()
  Description: This function is thread get timestamps.
  input: pointer to void.
  return : none.
*/
void *timestamp_function(void *thread_index)
{
	uint8_t T_index=4;	
	fstream input_file, output_file, ts;
	ostringstream name, name_1;
 	while(bool_flag)
	{
    	sem_wait(&semaphore[T_index]);
		sem_wait(&semaphore_timestamp);
		sem_wait(&semaphore_for_time);
		uint8_t str[10];
		display(counter_arr[T_index],4, str); 
	    buff_begin[T_index] = milliseconds_time();
		name.str(" ");
		name_1.str(" ");

		//perform output and input of characters to/from files:
		//Ref:http://www.cplusplus.com/doc/tutorial/files/
		name<<"Image_No_"<<str<<".ppm";
		name_1<<"output_"<<str<<".ppm";
		input_file.open(name.str(), ios::in);
		output_file.open(name_1.str(), ios::out);
		ts.open("system.out", ios::in);
		output_file << "P6" << endl << "#Timestamp:" << asctime(timecur) << "#System:" << ts.rdbuf() << "#" << input_file.rdbuf(); //To add timestamp in the output file
		output_file.close();
		input_file.close();
		ts.close();
		Jitter(T_index);
		sem_post(&semaphore_timestamp);
  	}
	Jitter_val(T_index);
	pthread_exit(NULL);
}

/*Function Name: display()
  Description: This function used for zero padding.
  input: integer.
  return : none.
*/
void display(int disp, uint8_t w, uint8_t* store)
{

	uint8_t character[10];
	int verify=0, track=0;
	uint8_t* buffer_string = character;
	for(track=w; track>1; track--)
	{
		switch(track)
			{
				case 2: 
					{
						verify = 9;
						if(disp<= verify)
						{
							*(store++) = '0';
						}
						break;
					}

				case 3:
					{
						verify = 99;
						if(disp<= verify)
						{
							*(store++) = '0';
						}
						break;
					}
				case 4:
					{
						verify = 999;
						if(disp<= verify)
						{
							*(store++) = '0';
						}
						break;
					}
				}
		}
		track = 0;
		do
		{
			*(++buffer_string) = '0' + disp%10;
			disp/=10;
			track++;
		}while(disp>0);
		for(;track>0;track--)
		{
			*(store++) = *(buffer_string--);
		}
		*(store) = 0;
			
						
	}


/*Function Name: delta_t()
  Description: This function calculates time.
  input: integepointer to struct timespec.
  return : none.
  Ref: This function is utilized from professor sam siewarts sequencer code.
*/

void delta_t(struct timespec *stop, struct timespec *start, struct timespec *delta_t)
{
  int dt_sec=stop->tv_sec - start->tv_sec;
  int dt_nsec=stop->tv_nsec - start->tv_nsec;

  if(dt_sec >= 0)
  {
    if(dt_nsec >= 0)
    {
      delta_t->tv_sec=dt_sec;
      delta_t->tv_nsec=dt_nsec;
    }
    else
    {
      delta_t->tv_sec=dt_sec-1;
      delta_t->tv_nsec=NSEC_PER_SEC+dt_nsec;
    }
  }
  else
  {
    if(dt_nsec >= 0)
    {
      delta_t->tv_sec=dt_sec;
      delta_t->tv_nsec=dt_nsec;
    }
    else
    {
      delta_t->tv_sec=dt_sec-1;
      delta_t->tv_nsec=NSEC_PER_SEC+dt_nsec;
    }
  }
  return;
}
	
/*Function Name: main()
  Description: This is main function of this file
  input: command line argument.
  return : integer
*/
int main(int argc, char *argv[])
{
	//To find the start time of the code.
	clock_gettime(CLOCK_REALTIME, &start_time); 

	printf("\n>>>>>>>>>>>>>>>>The start time is %d seconds and %d nanoseconds\n", start_time.tv_sec, start_time.tv_nsec);
	

	if(argc > 1)
	{
		sscanf(argv[1], "%d", &device);
	}
	
	//Setting up the width resolution for image viewer
	cap.set(CV_CAP_PROP_FRAME_WIDTH, HRES);

	//Setting up the height resolution for image viewer. 
	cap.set(CV_CAP_PROP_FRAME_HEIGHT, VRES);

	//Setting up the frame rate of camera.
	cap.set(CV_CAP_PROP_FPS,2000.0);

	//Initializing the camera device. 
	cap.open(device); 
	
	//Functions for the services.
	TL_Functions[0] = sequencer;
  	TL_Functions[1] = capture_frames_ppm;
  	TL_Functions[2] = save_encode_frame;
  	TL_Functions[3] = timestamp_function;

	//Initializing and creating threads.  
 	threads_init();

	//Releasing the camera device. 
	cap.release(); 

	//To find the end time of code.
	clock_gettime(CLOCK_REALTIME, &stop_time);
	printf("\nThe stop time is %d seconds and %d nanoseconds\n",stop_time.tv_sec, stop_time.tv_nsec); 
	
	//Calculating execution time of code.
	delta_t(&stop_time, &start_time, &execution);
	cout<<"\n\r The execution time for the code is: "<<execution.tv_sec<<" seconds "<<execution.tv_nsec<<" nano seconds.\n";
	cout<<"\n\rAll Images for Time-Lapse Captured";

	return 0;
}