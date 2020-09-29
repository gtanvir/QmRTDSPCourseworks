/*
 * assignment2_drums
 * ECS732 Real-Time DSP, 2019
 *
 * Second assignment, to create a sequencer-based
 * drum machine which plays sampled drum sounds in loops.
 *
 * This code runs on the Bela embedded audio platform (bela.io).
 *
 * Andrew McPherson, Becky Stewart and Victor Zappi
 * 2015-2019
 */


#include <Bela.h>
#include <cmath>
#include "drums.h"

/* Orientation States 
 */ 
#define INTERMEDIATE 0x06
#define FLAT 0x00
#define RIGHT 0x01
#define LEFT 0x02
#define UP 0x03
#define DOWN 0x04
#define OVER 0x05


/*  Orientation of the Board is initially FLAT  */
uint8_t systemState = FLAT;

extern float *gDrumSampleBuffers[NUMBER_OF_DRUMS];
extern int gDrumSampleBufferLengths[NUMBER_OF_DRUMS];

int gIsPlaying = 0;			/* Whether we should play or not. Implement this in Step 4b. */

/* Multiple pointers in a array to multiple drums 
 -1 means the corrsponding pointer is not being used to play anything 
 */ 
int gReadPointer = -1;
int gReadPointers[16] = {0};
int gDrumBufferForReadPointer[16] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

/* Patterns indicate which drum(s) should play on which beat.
 * Each element of gPatterns is an array, whose length is given
 * by gPatternLengths.
 */
extern int *gPatterns[NUMBER_OF_PATTERNS];
extern int gPatternLengths[NUMBER_OF_PATTERNS];

/* These variables indicate which pattern we're playing, and
 * where within the pattern we currently are.
 */
int gCurrentPattern = 0;				
int gCurrentIndexInPattern = 0;							

/* This variable holds the interval between events in **milliseconds**
 */
int gEventIntervalMilliseconds = 1000;
int gMetronomeCounter = 0;      				// Number of elapsed samples

/*This is the duration of LED being ON */ 
int gLEDinterval;

/* This variable indicates whether samples should be triggered or not. 
 */
extern int gIsPlaying;

/* This indicates whether we should play the samples backwards 
 */
int gPlaysBackwards = 0;

const int kButtonPin = 1; 				// digital pin P8_08, used as input for sensing button press 
const int kLed = 0;						// digital pin P8_07, used as output to control LED 
const int kIntervalInput = 0; 			// Analog 0, connected to potentiometer
const int kXAxisInput = 1; 				// Analog 1, connected to x pin of Accelerometer
const int kYAxisInput = 2; 				// Analog 2, connected to y pin of Accelerometer
const int kZAxisInput = 3; 				// Analog 3, connected to z pin of Accelerometer
uint16_t counter = 0;					// System Counter for using Accelerometer after an interval

int gPreviousButtonState = 1;			// Store the previous Button State 0 or 1 

bool setup(BelaContext *context, void *userData) {
	pinMode(context, 0, kButtonPin, INPUT);
	pinMode(context, 0, kLed, OUTPUT);
	return true;
}

void render(BelaContext *context, void *userData) {
	//Read the potentiometer
	float intervalPeriod = analogRead(context, 0, kIntervalInput); 		//The potentiometer controls the tempo 
	intervalPeriod = map(intervalPeriod, 0, 1, 50, 1000);				//Tempo Mapped into 50ms to 1000ms
	int gMetronomeInterval = (intervalPeriod/1000) * 44100;				//Convert Period in Milliseconds to Count Number
	
	// Led ON time is 80% of Metronome Interval 
	gLEDinterval = (int) gMetronomeInterval*0.8;						//LED ON duration
	
	counter ++;
	if(counter > 5000) {
		float xAxis = analogRead(context, 0, kXAxisInput);				// Accelerometer X axis value
		float yAxis = analogRead(context, 0, kYAxisInput);				// Accelerometer Y axis value
		float zAxis = analogRead(context, 0, kZAxisInput);				// Accelerometer Z axis value
		
		/* State Machine Code for detecting Orientation of the board */ 
		switch(systemState) {
			case FLAT: 
				if(zAxis < 0.52 )
					systemState = INTERMEDIATE;
					
			case RIGHT: 
				if(yAxis < 0.55)
					systemState = INTERMEDIATE;
					
			case LEFT:
				if(yAxis < 0.20)
					systemState = INTERMEDIATE;
				
			case UP:
				if(xAxis < 0.55)
					systemState = INTERMEDIATE;
					
			case DOWN:
				if(xAxis < 0.20)
					systemState = INTERMEDIATE;
					
			case OVER:
				if(zAxis < 0.15)
					systemState = INTERMEDIATE;
					
			case INTERMEDIATE: 
				gCurrentIndexInPattern = 0;
				if(yAxis > 0.55 && yAxis < 0.70)
					systemState = RIGHT;
				else if(yAxis > 0.20 && yAxis < 0.30)
					systemState = LEFT;
				else if(xAxis > 0.55 && xAxis < 0.70)
					systemState = UP;
				else if(xAxis > 0.20 && xAxis < 0.30)
					systemState = DOWN;
				else if(zAxis > 0.52 && zAxis < 0.70)
					systemState = FLAT;
				else if(zAxis > 0.15 && zAxis < 0.25)
					systemState = OVER;
					
		}
		counter = 0;
	}
		
	// Going through each frame 
	for (unsigned int n = 0; n < context->audioFrames; n++) {
		float out = 0; 			
		
		// Read the potentiometer 
		int status = digitalRead(context, n/2, kButtonPin);
		if(status == 0 && gPreviousButtonState != status)  					// Pressing the button causes the drum to Play or Stop  
			gIsPlaying =! gIsPlaying;

		//MetronomeCounter Code 
		if(++gMetronomeCounter >= gMetronomeInterval) { 
			// Counter reached the target            
			gMetronomeCounter = 0;          // Reset the counter            
			if(gIsPlaying == 1) {
			startNextEvent();
			digitalWriteOnce(context, n, kLed, HIGH);
			}
		}
		
		if(gMetronomeCounter > gLEDinterval) {             
			digitalWriteOnce(context, n, kLed, LOW);         
		} 
		
		//Playing multiple drums using multiple pointers
		for (int j = 0; j < 16; j++) {
			if(gDrumBufferForReadPointer[j] > -1) {
				if(gPlaysBackwards == 0) {									// Play forward
					out = gDrumSampleBuffers [gDrumBufferForReadPointer[j]] [gReadPointers[j]++];
					if(gReadPointers[j] == gDrumSampleBufferLengths[gDrumBufferForReadPointer[j]]) {
						gDrumBufferForReadPointer[j] = -1;					// Release the pointer 
						gReadPointers[j] = 0; 								// Reset the pointer 
					}
				}
				
				else if (gPlaysBackwards == 1) {							// Play backward
					//Something to do 
					out = gDrumSampleBuffers [gDrumBufferForReadPointer[j]] [gReadPointers[j]--];
					if(gReadPointers[j] == 0) {
						gDrumBufferForReadPointer[j] = -1;					// Release the pointer 
						gReadPointers[j] = gDrumSampleBufferLengths[j]-1; 	// Reset the pointer 
					}
				}
														
			}
		}
		
		//Playing the audio into hardware channels 
		for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
			// Write the sample to every audio output channel
    		audioWrite(context, n, channel, out);
    	}
		
		gPreviousButtonState = status; 
	}	
}

/* Start playing a particular drum sound given by drumIndex.
 */
void startPlayingDrum(int drumIndex) {
	for (int i = 0; i < 16; i++) {
		if(gDrumBufferForReadPointer[i] == -1)	{					// Pointer Not being used, so assign a drum to it  
			gDrumBufferForReadPointer[i] = drumIndex;	
			
			if(gPlaysBackwards == 0) {								// Reset the pointer value according to the directio of play (forward)
				gReadPointers[i] = 0;
			}
			else if (gPlaysBackwards == 1) {
				gReadPointers[i] = gDrumSampleBufferLengths[i]-1;	// Reset the pointer value according to the directio of play (backward)
			}
			break;
		}
	}
	
}

/* Start playing the next event in the pattern */
void startNextEvent() {
	if(systemState == OVER) 
		gPlaysBackwards = 1;
	else if(systemState != OVER) {
		gPlaysBackwards = 0;
		if(systemState != INTERMEDIATE)
			gCurrentPattern = systemState;
	}

	for(int j = 0; j < 8; j++) {
			if(eventContainsDrum(gPatterns[gCurrentPattern][gCurrentIndexInPattern], j) == 1) {
				startPlayingDrum(j);	
			}
		}		
	
	gCurrentIndexInPattern++;
	
	if(gCurrentIndexInPattern == gPatternLengths[gCurrentPattern])			// Pattern play finished, reset the index 
		gCurrentIndexInPattern = 0;
	
}

/* Returns whether the given event contains the given drum sound */
int eventContainsDrum(int event, int drum) {
	if(event & (1 << drum))
		return 1;
	return 0;
}

// cleanup_render() is called once at the end, after the audio has stopped.
// Release any resources that were allocated in initialise_render().

void cleanup(BelaContext *context, void *userData)
{

}
