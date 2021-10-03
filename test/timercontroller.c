// timecontrol rewrite to use arguments


#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include<time.h>
#include<stdbool.h> 

#define BUFFER_LENGTH 256               // The buffer length (crude but fine)

int fd, ret;

// Options and prototypes:
// -h --help : display help
void help(void);
// -r --read --rtc : display current real time and state of timer
int info(void);
// -tr --timer-reset X : set a new starting point for timer
int reset(char *starts);
// -tra --timer-reset-auto : automatically set a new starting point for timer
int startauto(void);
// -ts --timer-suspend : suspend the timer if it is running
int suspend(void);
// -ti --timer-interval X : set new interval period for timer
int interval(char *data);
// -p --pin-toggle: toggle pin manually
int toggle(void);
// -rs --rtc-set X : set rtc time to parameter
int timeset(char *stamp);

int main(int argc, char **argv){
    int error = 0;
    
    // Open connection to device
    fd = open("/dev/timerdev", O_RDWR);             // Open the device with read/write access
    if (fd < 0){
        perror("Failed to open the device...");
        return errno;
    }
    printf(">>Timerdev device opened successfully.\n");
    
    
    printf(">>Timer controller script is running...\n");
    // Parse options
    for (int i = 1; i < argc; i++)
    {  
        
            if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) // This is your parameter name
            {                 
                //char* filename = argv[i + 1];    // The next value in the array is your value
                //i++;    // Move to the next flag
                help();
            }
            if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--read") == 0 || strcmp(argv[i], "--rtc") == 0) // This is your parameter name
            {                 
                //char* filename = argv[i + 1];    // The next value in the array is your value
                //i++;    // Move to the next flag
                error = info();
            }
            if (strcmp(argv[i], "-tr") == 0 || strcmp(argv[i], "--timer-reset") == 0) // This is your parameter name
            {         
                if (i + 1 != argc){
                    char* timestamp = argv[i + 1];    // The next value in the array is your value
                    i++;    // Move to the next flag
                    error = reset(timestamp);
                } else {
                    printf(">>Error: Missing argument!");
                }
            }
			if (strcmp(argv[i], "-tra") == 0 || strcmp(argv[i], "--timer-reset-auto") == 0) // This is your parameter name
            {         
                error = startauto();
            }
            if (strcmp(argv[i], "-ts") == 0 || strcmp(argv[i], "--timer-suspend") == 0) // This is your parameter name
            {                 
                //char* timestamp = argv[i + 1];    // The next value in the array is your value
                //i++;    // Move to the next flag
                error = suspend();
            }
            if (strcmp(argv[i], "-ti") == 0 || strcmp(argv[i], "--timer-interval") == 0) // This is your parameter name
            {                 
                if (i + 1 != argc){
                    char* timeint = argv[i + 1];    // The next value in the array is your value
                    i++;    // Move to the next flag
                    error = interval(timeint);
                } else {
                    printf(">>Error: Missing argument!");
                }
            }
            if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--pin-toggle") == 0) // This is your parameter name
            {                 
                //char* timestamp = argv[i + 1];    // The next value in the array is your value
                //i++;    // Move to the next flag
                error = toggle();
            }
            if (strcmp(argv[i], "-rs") == 0 || strcmp(argv[i], "--rtc-set") == 0) // This is your parameter name
            {           
                if (i + 1 != argc){
                    char* timepoint = argv[i + 1];    // The next value in the array is your value
                    i++;    // Move to the next flag
                    error = timeset(timepoint);
                } else {
                    printf(">>Error: Missing argument!");
                }
            }
        
    }
    
    // No args? Default option
    if (argc ==1){
        startauto();
    }
    
    // Close program
    return error;
}

// Print out available commands and flags
void help(){
    printf(">>Timerc - Jaka Koren, 2020.\n\n");
    printf(">>Timerc script is intended to be used with timerdev kernel driver.\n");
    printf(">>It allows user to control a periodic pin toggle on BBB with the use of high resolution kernel timers.\n\n");
    printf("Switches:\n -h --help :  display help\n -r --read --rtc : display current real time (RTC) and information about timer\n");
    printf(" -tr --timer-reset X : set a new starting point for timer (requires a string for new starting point)\n");
	printf(" -tra --timer-reset-auto : reset timer with automatically determined new starting point\n");
    printf(" -ts --timer-suspend : suspend the timer if it is running\n -ti --timer-interval X : set new interval period for timer (requires a string for new interval period)\n");
    printf(" -p --pin-toggle X : toggle pin manually\n -rs --rtc-set X : set rtc time to parameter (requires a string for new date and time signature)\n");
    printf("\n");
    printf("\n");
}

// Read and display the current state of timer
int info(){
    char receive[BUFFER_LENGTH]; 
    printf(">>Reading device state...\n");
    ret = read(fd, receive, BUFFER_LENGTH);        // Read the response from the LKM
    if (ret < 0){
        perror(">>Failed to read the message from the device");
        return errno;
    }
    printf(">>The received message is: %s\n", receive);
    return 0;
}

// Set a new starting time point for toggle timer and reset
int reset(char *starts){
    printf(">>Writing restart command...\n");
    ret = write(fd, starts, strlen(starts)); // Send the string to the LKM
    if (ret < 0){
        perror(">>Failed to write the message to the device");
        return errno;
    }
    printf(">>Timer reset order sent successfully.\n");
    return 0;
}


// Sets new starting point for toggle timer automatically
int startauto(){
	// Read RTC
	printf(">>Attempting to reset timer automatically...\n");
    struct timespec mtime;
	char string[64];
	if( clock_gettime( CLOCK_REALTIME, &mtime) == -1 ) {
      fprintf(stderr, "Could not read current RTC time\n");
      return EXIT_FAILURE;
    }
    printf(">>RTC TIME WAS: %ld.%09ld\n", mtime.tv_sec, mtime.tv_nsec);
	// Create new timepoint (RTCtime / 10 + 1) * 10
	long long unsigned seconds = (long long unsigned) mtime.tv_sec;
	seconds = (seconds / 20 + 2) * 20;
	printf(">>NEW STARTPOINT WILL BE: %llu\n", seconds);
	sprintf(string, "%llu", seconds);
	// Set new timer start
	printf(">>Writing restart command...\n");
    ret = write(fd, string, strlen(string)); // Send the string to the LKM
    if (ret < 0){
        perror(">>Failed to write the message to the device");
        return errno;
    }
    printf(">>Timer reset order sent successfully.\n");
    return 0;
}


// Stop the timer if it is running
int suspend(void){
    printf(">>Writing suspend command...\n");
    ret = write(fd, "S", strlen("S")); // Send the string to the LKM
    if (ret < 0){
        perror(">>Failed to write the message to the device");
        return errno;
    }
    printf(">>Timer suspension order sent successfully.\n");
    return 0;
}

// Set new interval period for timer
int interval(char *data){
    char message[100];
    strcpy(message, "I ");
    strcat(message, data);
    
    printf(">>Writing new interval period [message]\n");
    ret = write(fd, message, strlen(message)); // Send the string to the LKM
    if (ret < 0){
        perror(">>Failed to write the message to the device");
        return errno;
    }
    printf(">>Timer starting point sent successfully.\n");
    return 0;
}

// Toggle pin manually
int toggle(){
    printf(">>Writing toggle command...\n");
    ret = write(fd, "P", strlen("P")); // Send the string to the LKM
    if (ret < 0){
        perror(">>Failed to write the message to the device");
        return errno;
    }
    printf(">>Timer starting point sent successfully.\n");
    return 0;
}

// Set rtc time to parameter
int timeset(char *stamp){
    printf(">>Attempting to set time...\n");
    struct timespec mtime, stime;
    time_t newtime = 0;
    long int newnsecs = 0;
    
    int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;
    
    // First argument processing
    if (sscanf(stamp, "%4d.%2d.%2d %2d:%2d:%2d:%9ld", &year, &month, &day, &hour, &min, &sec, &newnsecs) == 7){
        
        struct tm broke = {0};
        broke.tm_year = year - 1900; /* years since 1900 */
        broke.tm_mon = month - 1;
        broke.tm_mday = day;
        broke.tm_hour = hour;
        broke.tm_min = min;
        broke.tm_sec = sec;
        
        if ((newtime = mktime(&broke)) == (time_t)-1) {
          fprintf(stderr, "Could not convert time input to time_t\n");
          return EXIT_FAILURE;
        }
    } else {
        fprintf(stderr, "Time input was not in valid form\n");
        return EXIT_FAILURE;
    }
    
    stime.tv_sec = newtime;
    stime.tv_nsec = newnsecs;
    
    if( clock_gettime( CLOCK_REALTIME, &mtime) == -1 ) {
      fprintf(stderr, "Could not read current RTC time\n");
      return EXIT_FAILURE;
    }
    printf(">>RTC TIME WAS: %d.%09d\n", &mtime.tv_sec, &mtime.tv_nsec);
    
     if( clock_settime( CLOCK_REALTIME, &stime) == -1 ) {
       fprintf(stderr, "Could not set new time for RTC\n");
       return EXIT_FAILURE;
    }
    printf(">>RTC SET TO: %d.%09d\n", &stime.tv_sec, &stime.tv_nsec);
    return 0;
}
