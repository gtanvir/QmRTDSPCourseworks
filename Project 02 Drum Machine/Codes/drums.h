/*
 * assignment2_drums
 *
 * Second assignment for ECS732 RTDSP, to create a sequencer-based
 * drum machine which plays sampled drum sounds in loops.
 *
 * This code runs on BeagleBone Black with the Bela environment.
 *
 * 2016, 2017
 * Becky Stewart
 *
 * 2015
 * Andrew McPherson and Victor Zappi
 * Queen Mary University of London
 */


#ifndef _DRUMS_H
#define _DRUMS_H

#define NUMBER_OF_DRUMS 8
#define NUMBER_OF_PATTERNS 6
#define FILL_PATTERN 5

/* Start playing a particular drum sound */
void startPlayingDrum(int drumIndex);

/* Start playing the next event in the pattern */
void startNextEvent();

/* Returns whether the given event contains the given drum sound */
int eventContainsDrum(int event, int drum);

#endif /* _DRUMS_H */
