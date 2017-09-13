#include <stdio.h>
#include <string.h>
#include "buspirate.h"

/*
 * Very simple bootloader for ARAFE Master reprogramming
 * This one is implemented using the Bus Pirate libraries for when we don't have an ATRI board lying around
 * 
 * Usage
 *
 * type "./busbsl program file.name" where file.name is the name of the binary file you want to program.
 *
 * You can also type "./busbsl erase" to erase the memory without reprogramming. This will require a reboot before programming. 
 * 
 * By Brian Clark (clark.2668@osu.edu), 2017, The Ohio State University
 */
 
int v =1; //variable to control verbosity of the output, because c doesn't supper "true"/"false" as booleans...
//1 = verbose, 0 = not verbose (!0 = true,  0 = false)
//naughty thing to do with a global variable, but oh well...

void main(int argc, char **argv){
	
	argc--; argv++;
	if(!argc) {printf("I need a command! Usage is \"./busbsl command [optional-args]\" .\n"); exit(1); }
	
	while(argc){ //okay, loop over arguments
		if (strstr(*argv, "program")){ //if the command is to program the ARAFE master
			if(argc<2){ //check if they've actually given you a firmware file to use
				printf("You need to give me a firmware file to program! \n"); //tell them they messed up
				exit(1); //get out
			}
			
			if(v) printf("I'm going to try and program the ARAFE master...\n"); //announce that the programming will be attempted
			
			printf("file name to be loaded: %s\n", argv[1]); //print out the filename we are going to use
			int file = open(argv[1], O_RDONLY); //open the file you passed me
			if(!file){ //check to make sure the file is open
				printf("Something went wrong with opening the file!\n"); //tell them something went wrong
				exit(0); //get out
			}
			if(v) printf("Opening the file was successful\n");
			
			exit(0); //get out
		}
		else{
			printf("I can't find a command to issue. Exiting without doing anything.\n"); //tell the user we're not going to do anything
			exit(1); //get out
		}
	}
}
