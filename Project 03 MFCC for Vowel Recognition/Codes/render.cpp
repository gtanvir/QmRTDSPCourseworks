// ECS732 Real-Time DSP
// School of Electronic Engineering and Computer Science
// Queen Mary University of London
// Spring 2019
// Final Project - Bengali Vowel recogntion
// Student No: 180715305 

#include <Bela.h>
#include <ne10/NE10.h>
#include <cmath>
#include <cstring>
#include <SampleLoader.h>
#include <SampleData.h>

#define BUFFER_SIZE 512 

#define numberOfFilterPoints 28
#define totalNumberOfFilters 26
#define totalLength 186

float gWindowBuffer[BUFFER_SIZE]; // Buffer for 1 window of samples
int gWindowBufferPointer = 0;     // Tells us how many samples in buffer

// -----------------------------------------------
// These variables used internally in the example:
int gFFTSize;

// GPIO
int gOutputPinLo = 0, gOutputPinHi = 1;	 // LEDs
int gLEDLoOutput = LOW, gLEDHiOutput = LOW;

// Sample info
string gFilename = "M3AA.wav"; 		// Name of the sound file (in project folder)
float *gSampleBuffer;			 	// Buffer that holds the sound file
int gSampleBufferLength;		 	// The length of the buffer in frames
int gReadPointer = 0;			 	// Position of the last frame we played 

// FFT vars
ne10_fft_cpx_float32_t* timeDomainIn;
ne10_fft_cpx_float32_t* timeDomainOut;
ne10_fft_cpx_float32_t* frequencyDomain;
ne10_fft_cfg_float32_t cfg;

// Thread for FFT processing
AuxiliaryTask gFFTTask;

void process_fft_background(void *);

//Filter Bank Bandpoints 
int fbBandPoints[numberOfFilterPoints] = {5,7,10,12,14,17,20,23,26,30,34,38,43,48,54,60,66,73,81,89,98,107,118,129,141,155,169,185};
float filterBank[totalNumberOfFilters][totalLength];

//Power, Filter Energy and DCT 
float powerSpectrum[BUFFER_SIZE/2+1] = {0}; 
float filterEnergy[totalNumberOfFilters] = {0}; 
float dctFilterEnergy[totalNumberOfFilters];
float mfcc[13];
float sum = 0;

//Preemphasis
float gPreviousInput = 0;
float out = 0; 

// setup() is called once before the audio rendering starts.
// Return true on success; returning false halts the program.

bool setup(BelaContext *context, void *userData)
{
	// Check the length of the audio file and allocate memory
    gSampleBufferLength = getNumFrames(gFilename);
    
    if(gSampleBufferLength <= 0) {
    	rt_printf("Error loading audio file '%s'\n", gFilename.c_str());
    	return false;
    }
    
    gSampleBuffer = new float[gSampleBufferLength];
    
    // Make sure the memory allocated properly
    if(gSampleBuffer == 0) {
    	rt_printf("Error allocating memory for the audio buffer.\n");
    	return false;
    }
    
    // Load the sound into the file (note: this example assumes a mono audio file)
    getSamples(gFilename, gSampleBuffer, 0, 0, gSampleBufferLength);

    rt_printf("Loaded the audio file '%s' with %d frames (%.1f seconds)\n", 
    			gFilename.c_str(), gSampleBufferLength,
    			gSampleBufferLength / context->audioSampleRate);	

	// Set up the FFT
	gFFTSize = BUFFER_SIZE;

	timeDomainIn = (ne10_fft_cpx_float32_t*) NE10_MALLOC (gFFTSize * sizeof (ne10_fft_cpx_float32_t));
	timeDomainOut = (ne10_fft_cpx_float32_t*) NE10_MALLOC (gFFTSize * sizeof (ne10_fft_cpx_float32_t));
	frequencyDomain = (ne10_fft_cpx_float32_t*) NE10_MALLOC (gFFTSize * sizeof (ne10_fft_cpx_float32_t));
	cfg = ne10_fft_alloc_c2c_float32_neon (gFFTSize);

	memset(timeDomainOut, 0, gFFTSize * sizeof (ne10_fft_cpx_float32_t));
	
	// Set up the thread for the FFT
	gFFTTask = Bela_createAuxiliaryTask(process_fft_background, 50, "bela-process-fft");
	
	// Calculate a Hann window
	for(int n = 0; n < gFFTSize; n++) {
		gWindowBuffer[n] = 0.5f * (1.0f - cosf(2.0 * M_PI * n / (float)(gFFTSize - 1)));
	}
	
	//Calculating Mel Triangle Overlapping Filters 
	for (int i=0; i<totalNumberOfFilters; i++) {
		for (int j=0; j<totalLength; j++) {
			if(j<fbBandPoints[i] || j>fbBandPoints[i+2])
				filterBank[i][j] = 0;
			else if (j>=fbBandPoints[i] && j<fbBandPoints[i+1]) 
				filterBank[i][j] = (float) (j-fbBandPoints[i])/(fbBandPoints[i+1]-fbBandPoints[i]);
			else if(j == fbBandPoints[i+1])
				filterBank[i][j] = 1; 
			else if (j>fbBandPoints[i+1] && j<=fbBandPoints[i+2])
            	filterBank[i][j] = (float) (fbBandPoints[i+2]-j)/(fbBandPoints[i+2]-fbBandPoints[i+1]);
		}
	}
	
	return true;
}

// This function handles the FFT processing in this example once the buffer has
// been assembled.
void process_fft(float *buffer)
{
	// Copy buffer into FFT input
	for(int n = 0; n < BUFFER_SIZE; n++) {
		timeDomainIn[n].r = (ne10_float32_t) buffer[n] * gWindowBuffer[n];
		timeDomainIn[n].i = 0;
	}
	
	// Run the FFT
	ne10_fft_c2c_1d_float32_neon (frequencyDomain, timeDomainIn, cfg, 0);
	
	//Calculating Power 
	for (int i = 0; i < (1+BUFFER_SIZE/2); i++) {
		powerSpectrum[i] = ((frequencyDomain[i].r * frequencyDomain[i].r) + (frequencyDomain[i].i * frequencyDomain[i].i)) / BUFFER_SIZE;	
	}
	
	//Calculating Energy in each of the 26 Mel filters 
	for(int i=0; i<totalNumberOfFilters; i++) {
		for(int j=(fbBandPoints[i]+1); j<fbBandPoints[i+2]; j++) {
			filterEnergy[i] = filterEnergy[i] + powerSpectrum[j] * filterBank[i][j]; 
		}	
	}
	
	//Calculating Log Filter Energy 
	for(int i=0; i<totalNumberOfFilters; i++) {
		filterEnergy[i] = logf(filterEnergy[i]);
	}
	
	//MFCC calculation by DCT 
	for(int i=0; i<totalNumberOfFilters; i++) {
		for(int j=0; j<totalNumberOfFilters; j++) {
			//dctFilterEnergy[i] = 1;
			dctFilterEnergy[i] = dctFilterEnergy[i] + filterEnergy[j] * cosf((M_PI/totalNumberOfFilters)*(j+0.5)*i);	
		}
	}
	
	for(int i=1; i<13; i++) {
		rt_printf("%0.2f ", dctFilterEnergy[i]);
	}
	rt_printf("\n\r");
	
	//-------------------Algorithm of vowel recogntion should be here ------------------//
	//-------------------------------NOT IMPLEMENTED------------------------------------//
	
	//Clearing up the values of Power, filterEnergy and MFCC current Frame to store the next coming one 
	for(int i=0; i<(1+BUFFER_SIZE/2); i++) {
		powerSpectrum[i] = 0;
	}
	
	for(int i=0; i<totalNumberOfFilters; i++) {
		filterEnergy[i] = 0;
		dctFilterEnergy[i] = 0; 
	}
}

// This function runs in an auxiliary task on Bela, calling process_fft
void process_fft_background(void *)
{
	process_fft(gWindowBuffer);
}

// render() is called regularly at the highest priority by the audio engine.
// Input and output are given from the audio hardware and the other
// ADCs and DACs (if available). If only audio is available, numMatrixFrames
// will be 0.

void render(BelaContext *context, void *userData)
{
	for(unsigned int n = 0; n < context->audioFrames; n++) {
        //float in = gSampleBuffer[gReadPointer];
        float in = audioRead(context, n, 0);
        
        out = in - 0.95 * gPreviousInput;		//Preemphasis 
        
        /*
        if(++gReadPointer >= gSampleBufferLength)
        	gReadPointer = 0;
		*/
		
		gWindowBuffer[gWindowBufferPointer++] = in;
		//When Buffer is full, initiate the FFT task 
		if(gWindowBufferPointer == BUFFER_SIZE) {
			gWindowBufferPointer = 0;
			Bela_scheduleAuxiliaryTask(gFFTTask);
		}

		// Copy input to output
		for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
			audioWrite(context, n, channel, in);
		}
		
		gPreviousInput = in; 
	}
}

// cleanup() is called once at the end, after the audio has stopped.
// Release any resources that were allocated in setup().

void cleanup(BelaContext *context, void *userData)
{
	NE10_FREE(timeDomainIn);
	NE10_FREE(timeDomainOut);
	NE10_FREE(frequencyDomain);
	NE10_FREE(cfg);
}
