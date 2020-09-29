// Assignment 1: synth filter
//
// ECS732 Real-Time DSP
// School of Electronic Engineering and Computer Science
// Queen Mary University of London
// Spring 2019

#include <Bela.h>
#include <SampleLoader.h>
#include <Gui.h>
#include <cmath>

string gFilename = "guitar.wav"; // Name of the sound file (in project folder)

float *gSampleBuffer;			 // Buffer that holds the sound file
int gSampleBufferLength;		 // The length of the buffer in frames
int gReadPointer = 0;			 // Position of the last frame we played

float filterFrequency;			 // CutOff frequency of Filter 
float filterQ; 					 // Q-Factor 

// browser-based GUI to adjust parameters
Gui sliderGui;

// ****************************************************************
// TODO: declare your global variables here for coefficients and filter state
// ****************************************************************

float x[3] = {0,0,0}, out1[3] = {0,0,0}, out2[3] = {0,0,0};
float a0, a1, a2, b0, b1, b2; 
int p=2;


// Calculate filter coefficients given specifications
void calculate_coefficients(float sampleRate, float frequency, float q)
{
	
	/*
	fc = Cutoff frequency
	k = SamplingPeriod/2
	q = Quality Factor 
	
			       (k^2)*(wc^2) 		+ 	2*(k^2)*(fc^2) * z^(-1) 			+ (k^2)*(wc^2)*z^(-2)
	H(z) = ------------------------------------------------------------------------------------------------------------
	        (1 + wc*k/q + (k^2)*(fc^2))	+	2*((k^2)*(wc^2)-1) * z^(-1)	  		+ (1 - wc*k/q + (k^2)*(fc^2)) * z^(-2)
	
	*/
	
	
	float T = 1/sampleRate; 
	float k = T/2;
	float kSquare = k*k;
 	float w = 2*M_PI*frequency;
 	float wSquare = w*w;
	float wkq = w*k/q;
	float squareOfkw = kSquare * wSquare;
	float numerator = 1+wkq+squareOfkw;
	
	
	a0 = 1;
	a1 = 2*(squareOfkw-1)/numerator;
	a2 = (1-wkq+squareOfkw)/numerator;
	
	b0 = squareOfkw/numerator;
	b1 = 2*b0;
	b2 = b0;
	
}

// Read an interpolated sample from the wavetable
float wavetable_read(float sampleRate, float frequency)
{
	// The pointer will take a fractional index. Look for the sample on
	// either side which are indices we can actually read into the buffer.
	// If we get to the end of the buffer, wrap around to 0.
	int indexBelow = floorf(gReadPointer);
	int indexAbove = indexBelow + 1;
	if(indexAbove >= gSampleBufferLength)
		indexAbove = 0;
	
	// For linear interpolation, we need to decide how much to weigh each
	// sample. The closer the fractional part of the index is to 0, the
	// more weight we give to the "below" sample. The closer the fractional
	// part is to 1, the more weight we give to the "above" sample.
	float fractionAbove = gReadPointer - indexBelow;
	float fractionBelow = 1.0 - fractionAbove;
	
	// Calculate the weighted average of the "below" and "above" samples
    float out = (fractionBelow * gSampleBuffer[indexBelow] + fractionAbove * gSampleBuffer[indexAbove]);

    // Increment read pointer and reset to 0 when end of table is reached
    gReadPointer += gSampleBufferLength * frequency / sampleRate;
    while(gReadPointer >= gSampleBufferLength)
        gReadPointer -= gSampleBufferLength;
        
    return out;
}

bool setup(BelaContext *context, void *userData)
{
	/*
	----------------------------- Input = Wavetable---------------------------
	--------------------------------------------------------------------------
	// Generate a sawtooth wavetable (a ramp from -1 to 1)
	for(unsigned int n = 0; n < gSampleBufferLength; n++) {
		gSampleBuffer[n] = -1.0 + 2.0 * (float)n / (float)(gSampleBufferLength - 1);
	}
	*/

	/*
	----------------------------- Input = Wavefile ---------------------------
	--------------------------------------------------------------------------
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
	*/ 
	
	
	
	//----------------------------- GUI Setup ---------------------------
	// Set up the GUI
	sliderGui.setup(5432, "gui");
	
	// Arguments: name, minimum, maximum, increment, default value
	// Create sliders for oscillator and filter settings
	sliderGui.addSlider("Oscillator Frequency", 55, 440, 55, 220);
	sliderGui.addSlider("Oscillator Amplitude", 0, 1, 0, 1);
	sliderGui.addSlider("Filter Cutoff Frequency", 125, 4000, 125, 1500);
	sliderGui.addSlider("Filter Q", 0.5, 10, 0.5, 2);
	
	
	return true;
}

void render(BelaContext *context, void *userData)
{
	
	//----------------------------- Input from GUI  ---------------------------
	//float oscFrequency = sliderGui.getSliderValue(0);	
	//float oscAmplitude = sliderGui.getSliderValue(1);	
	//float filterFrequency = sliderGui.getSliderValue(2);
	//float filterQ = sliderGui.getSliderValue(3);
	
	
	float powOfFrequency = map(analogRead(context, 0, 0), 0, 1, 2, 3.6);	// 100Hz ~ 4000Hz, 10^2 = 100, 10^3.6 = 4000;
	filterFrequency = pow(10, powOfFrequency);								// Converting to Log Scale
	filterQ = map(analogRead(context, 0, 1), 0, 1, 0.5, 10);				// Range of Q: 0.5~10
	
	// Calculate new filter coefficients
	calculate_coefficients(context->audioSampleRate, filterFrequency, filterQ);

    
    
    for(unsigned int n = 0; n < context->audioFrames; n++) {
    	
		//x[p] = oscAmplitude * wavetable_read(context->audioSampleRate, oscFrequency);		// Input from Wavetable 
        //x[p] = gSampleBuffer[gReadPointer];												// Input from Wavefile
        
        
        
        
        x[p] =  audioRead(context, n, 0);													// Input from audioRead
        
        // ------------------------------FILTER IMPLEMENT CODE HERE------------------------------// 
        
        out1[p] =   b0 * x[p]    + b1 * x[p-1] 	  + b2 * x[p-2]    - a1 * out1[p-1] - a2 * out1[p-2] ;			
        out2[p] =   b0 * out1[p] + b1 * out1[p-1] + b2 * out1[p-2] - a1 * out2[p-1] - a2 * out2[p-2] ;
        
        // Write the output to every audio channel
    	for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
    		audioWrite(context, n, channel, out2[p]);
    	}
    	
  
    	//Storing the value of variables for next render loop
    	x[p-2] = x[p-1];													// x(n-2) = x(n-1)
		x[p-1] = x[p];														// x(n-1) = x(n)
		
		out1[p-2] = out1[p-1];												// out1(n-2) = out1(n-1)
		out1[p-1] = out1[p];												// out1(n-1) = out1(n)
		
		out2[p-2] = out2[p-1];												// out2(n-2) = out2(n-1)
	    out2[p-1] = out2[p];												// out2(n-1) = out2(n)
	    
	    
	    //----------------------------- Wrapping up for Wavetable or Wavefile ------------------------------FILTER
	    /*
	    if(++gReadPointer >= gSampleBufferLength)							// Wrapping Up 
        	gReadPointer = 0;
        */	
    }
}

void cleanup(BelaContext *context, void *userData)
{
	// delete[] gSampleBuffer;
}
