// Timertest: We initiate a hi-resolution timer after a delay
// which then prints an output on interval


/*
 * Test kernel module for device drivers - communication with the thingy from a c program
 * 
 *
 * make - produce timertest.ko kernel device
 * lsmod - list currently loaded kernel devices
 * sudo insmod timerdev.ko - loads a device into kernel
 * sudo rmmod timerdev - unloads a device
 *
 * /dev directory - pairs of major/minor numbers assigned to a device
 * /var/log/kern.log - log of kernel operations
 * sudo tail -f /var/log/kern.log
 * 
 * Use in conjunction with timerc from userspace
 * 
 */

#include <linux/init.h> // Function markup macros
#include <linux/module.h> // Header for loading LKMs into kernel
#include <linux/device.h> // Header for support of device driver model
#include <linux/kernel.h> // Macros, types and functions for kernel
#include <linux/hrtimer.h> // Header for hrtimers
#include <linux/ktime.h> // Header for real time formats and structs
#include <linux/delay.h> // Header for delay functions
#include <linux/slab.h> // For slab allocation
#include <linux/fs.h> // Header for linux file system support
#include <linux/uaccess.h> // Required for copy to user function
#include <linux/gpio.h> // Required for gpio control

#define DEVICE_NAME "timerdev" // This will make this device appear in /dev/timerdev under this value
#define CLASS_NAME "timdev" // Device class - a character device driver


MODULE_LICENSE("GPL"); ///< Licence type affects runtime behaviour. GPL is most free.
MODULE_AUTHOR("Jaka Koren");
MODULE_DESCRIPTION("Test device driver to run high res timers on beaglebone black.");
MODULE_VERSION("0.5");

/// ================ Messaging specs


static int majorNumber; // Stores device number automatically
static char message[256] = {0}; // Empty memory for string that will be sent from userspace
static short msgsize; // To remember the size of message
static int numberOpens = 0; // Counts the number of times the device is opened
static struct class* sendcharClass  = NULL; // The device-driver class struct pointer
static struct device* sendcharDevice = NULL; // The device-driver device struct pointer



// Prototype functions
static int      dev_open(struct inode *, struct file *);
static int      dev_release(struct inode *, struct file *);
static ssize_t  dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t  dev_write(struct file *, const char *, size_t, loff_t *);
static int		timer_restart( int startpoint_s, int startpoint_ns);
static int		timer_interval_set(char * setting);
static void     timer_stop(void);
static void     pin_toggle(void);

// Device is represented as a file structure in the kernel. We need to predefine
// additional functions for manipulation with user space. For full list go to /linux/fs.h
static struct file_operations fops =
{
   .open = dev_open, // Called each time the device is opened from user space
   .read = dev_read, // Called when data is sent from device to user space
   .write = dev_write, // Called when data is sent from user to device
   .release = dev_release, // Called when device is closed in user space
};

/// ================ GPIO Pin specs

static unsigned int gpioPin = 66; /// Right side, fourth from top, left column
static bool timerRunning = 0;
static bool pinOn = 0;

/// ================ Timer interval specs

long timer_interval_s = 60; // Timer interval in seconds
unsigned long timer_interval_ns = 0; // Timer interval in nanoseconds / 1e9 = 1 second

long start_interval_s = 5; // Timer interval in seconds
unsigned long start_interval_ns = 0; // Timer interval in nanoseconds


static struct hrtimer hr_timer;
ktime_t currtime , interval, rtctime, diffinterval;

/// ================ Timer callback

enum hrtimer_restart timer_callback( struct hrtimer *timer_for_restart )
{
  	currtime  = ktime_get(); // Current time
  	rtctime = ktime_get_real(); // Current time from real time clock
  	
  	// Toggle pin
  	pinOn = !pinOn;
	gpio_set_value(gpioPin, pinOn);
  	
  	//printk(KERN_INFO "TimerDev: Tick at %d.%09d, pin value on %d\n", ktime_to_timeval(currtime), ktime_to_ns(currtime), pinOn);
  	printk(KERN_INFO "TimerDev: RTC  at %d.%09d\n", ktime_to_timeval(rtctime), ktime_to_ns(rtctime));
  	//diffinterval = ktime_sub(rtctime, currtime);
  	//printk(KERN_INFO "TimerDev: Diff is %d.%09d\n", ktime_to_timeval(diffinterval), ktime_to_ns(diffinterval));
  	
  	interval = ktime_set(timer_interval_s,timer_interval_ns); // New interval set from number of seconds and number of nanoseconds
  	hrtimer_forward(timer_for_restart, currtime , interval); // Make timer restart
  	
	return HRTIMER_RESTART;
}

/// ================ Initialization

static int __init timer_init(void) {
	// Prepare device
	printk(KERN_INFO "TimerDev: Initializing LKM\n");
	
    // Try to dynamically allocate a major number for the device -- more difficult but worth it
    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if (majorNumber<0){
       printk(KERN_ALERT "SendChar failed to register a major number\n");
       return majorNumber;
    }
    printk(KERN_INFO "TimerDev: registered correctly with major number %d\n", majorNumber);

    // Register the device class
    sendcharClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(sendcharClass)){                // Check for error and clean up if there is
       unregister_chrdev(majorNumber, DEVICE_NAME);
       printk(KERN_ALERT "Failed to register device class\n");
       return PTR_ERR(sendcharClass);          // Correct way to return an error on a pointer
    }
    printk(KERN_INFO "TimerDev: device class registered correctly\n");

    // Register the device driver
    sendcharDevice = device_create(sendcharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(sendcharDevice)){               // Clean up if there is an error
       class_destroy(sendcharClass);           // Repeated code but the alternative is goto statements
       unregister_chrdev(majorNumber, DEVICE_NAME);
       printk(KERN_ALERT "Failed to create the device\n");
       return PTR_ERR(sendcharDevice);
    }
    printk(KERN_INFO "TimerDev: device class created correctly\n"); // Made it! device was initialized
	
	// Set up pin
	if (!gpio_is_valid(gpioPin)){
      printk(KERN_INFO "TimerDev: invalid LED GPIO\n");
      return -ENODEV;
    }
    
    gpio_request(gpioPin, "sysfs");
    gpio_direction_output(gpioPin, pinOn);   // Set the gpio to be in output mode and off
    gpio_export(gpioPin, false);
	
	// Set up and initiate first timer
	currtime  = ktime_get(); // Current time from boot
	rtctime = ktime_get_real(); // Current time from real time clock
  	printk(KERN_INFO "TimerDev: Start   %d.%09d\n", ktime_to_timeval(currtime), ktime_to_ns(currtime));
  	printk(KERN_INFO "TimerDev: RTC  at %d.%09d\n", ktime_to_timeval(rtctime), ktime_to_ns(rtctime));
  	
  	diffinterval = ktime_sub(rtctime, currtime);
  	printk(KERN_INFO "TimerDev: Diff is %d.%09d\n", ktime_to_timeval(diffinterval), ktime_to_ns(diffinterval));
  	
    ktime_t ktime;
	ktime = ktime_set(start_interval_s, start_interval_ns ); // Create a variable for starting time interval
	
	
	hrtimer_init( &hr_timer, CLOCK_REALTIME, HRTIMER_MODE_REL ); // Initiate a hi-res timer with realtime clock
	// Realtime clock may have changes when system time is altered
	hr_timer.function = &timer_callback; // Function callback on timer interval
	
	printk(KERN_INFO "Timerdev: Delaying\n");
	ndelay(1e6); // Delay for nanoseconds
	
	rtctime = ktime_get_real();
	currtime = ktime_get();
	
 	hrtimer_start( &hr_timer, ktime, HRTIMER_MODE_REL ); // Start timer
 	printk(KERN_INFO "Timerdev: Timer initiated\n");
 	timerRunning = true;
	return 0;
}

/// ================ Exit

static void __exit timer_exit(void) {
	// Disable timer if still running
	int ret;
  	ret = hrtimer_cancel( &hr_timer );
  	if (ret) printk("Timerdev: The timer was in use.\n");
  	timerRunning = false;
  	
  	// Set pins to low
  	gpio_set_value(gpioPin, 1);
  	gpio_unexport(gpioPin);
  	gpio_free(gpioPin);
  	
  	
  	// LKM cleanup
  	device_destroy(sendcharClass, MKDEV(majorNumber, 0));     // remove the device
    class_unregister(sendcharClass);                          // unregister the device class
    class_destroy(sendcharClass);                             // remove the device class
    unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
  	
  	printk("======== Uninstalling timerdev module.\n");
	
}

/// ================ Receiver/sender functions

// Called whenever the user opens this device 
// Increments the counter and prints
// inodep - pointer to an inode object
// filep - pointer to a file
static int dev_open(struct inode *inodep, struct file *filep){
   numberOpens++;
   printk(KERN_INFO "Timerdev: Device has been opened %d time(s)\n", numberOpens);
   return 0;
}

// Called when data is sent from device to user. 
// copy_to_user sends the buffer string to the user and captures errors
// filep - pointer to file object
// buffer - pointer to the buffer in which the data is being written
// len - length of the message
// offset - if required
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
   int error_count = 0;
   // copy_to_user has the format ( * to, *from, size) and returns 0 on success
   
   // On first call the thing does not send anything - message buffer issue?
   rtctime = ktime_get_real();
   // The message will consist of current realtime info - modify in timer callback
   //memset(message,0,sizeof(message));
   sprintf(message, "\nRTC: %d.%09d\nTIMER: %d \nINTERVAL: %ld %09ld\n", ktime_to_timeval(rtctime), ktime_to_ns(rtctime), timerRunning, timer_interval_s, timer_interval_ns);
   msgsize = strlen(message);
   error_count = copy_to_user(buffer, message, msgsize);
 
   if (error_count==0){            // if true then have success
      printk(KERN_INFO "Timerdev: Sent message: %s (%d)\n", message, msgsize);
      memset(message,0,sizeof(message));
      return (msgsize=0);  // clear the position to the start and return 0
   }
   else {
      printk(KERN_INFO "Timerdev: Failed to send %d characters to the user\n", error_count);
      memset(message,0,sizeof(message));
      return -EFAULT;              // Failed -- return a bad address message (i.e. -14)
   }
   
}

// Called when user writes something to the device. Data is copied to message[] array, along with length of the string
// filep - pointer to file object
// buffer - buffer that contains the string to write to the device
// len - length of the array that is being passed
// offset - if required
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
    int error_count = 0;
    
    long long int seconds;
    error_count = copy_from_user(message, buffer, len);
    if (error_count == 0){
        printk(KERN_INFO "Timerdev: Received buffer [%s] Length: %d First char %c \n", message, len, message[0]);
    } else {
        printk(KERN_INFO "Timerdev: Failed to receive characters from the user \n");
        return -EFAULT;
    }
    
    
    if (message[0]== 'I'){
        // Set new toggle interval
        printk(KERN_INFO "Timerdev: Setting new interval time \n");
        error_count = timer_interval_set(message);
        if (error_count != 0){
            printk(KERN_INFO "Timerdev: Failed to receive characters from the user\n");
            return -EFAULT;
        }
    } else if (message[0]== 'P'){
        // Manually toggle pin
        pin_toggle();
    } else if (message[0] == 'S'){
        // Stop timer manually
        timer_stop();
    } else {
        // Default: set new time starting point 
        error_count = kstrtoull_from_user(buffer, len, 10, &seconds);
        if (error_count == 0){
            printk(KERN_INFO "Timerdev: Received startpoint of %lld seconds\n", seconds);
            error_count = timer_restart(seconds, 0);
            return error_count;
        } else {
            printk(KERN_INFO "Timerdev: Failed to receive characters from the user\n");
            return -EFAULT;
        }
    }
    return len;
}

// Called when userspace program closes/releases the device
// inodep - pointer to inode object
// filep - pointer to a file object
static int dev_release(struct inode *inodep, struct file *filep){
   printk(KERN_INFO "Timerdev: Device successfully closed\n");
   return 0;
}

// Called to restart timer with new start point
static int timer_restart(int startpoint_s, int startpoint_ns){
	// Stop timer
	int ret;
  	ret = hrtimer_cancel( &hr_timer );
  	if (ret) printk("Timerdev: The timer was in use.\n");
  	timerRunning = false;
  	
  	// Set pin to low
  	pinOn = 0;
  	gpio_set_value(gpioPin, pinOn);
  	
  	currtime  = ktime_get(); // Current time
  	rtctime = ktime_get_real(); // Current time from real time clock
  	printk(KERN_INFO "TimerDev: Restarting  %d.%09d\n", ktime_to_timeval(currtime), ktime_to_ns(currtime));
  	printk(KERN_INFO "TimerDev: RTC restart %d.%09d\n", ktime_to_timeval(rtctime), ktime_to_ns(rtctime));
  	
  	
  	ktime_t newtime;
  	newtime = ktime_set(startpoint_s, startpoint_ns);
  	
  	diffinterval = ktime_set(0, 100); //attempt to achieve higher accuracy;
  	newtime = ktime_sub(newtime, diffinterval);
  	
  	// Is startpoint lower than current rtc?
  	//if (ktime_to_timeval(rtctime) > startpoint_s){ // NEEDS A DIFFERENT COMPARISON FUNCTIOn
  	//	printk(KERN_INFO "TimerDev: Restart time point too low - not reinitializing timer.");
  	//}
  	
  	// If not, calculate diff, reinitialize
  	bool after = 0;
  	after = ktime_after(newtime, rtctime);
  	
  	// Timer starts about 100 nanoseconds after it should..
  	
  	if (after){
      	
      	rtctime = ktime_get_real(); // Current time from real time clock
      	
      	hrtimer_start( &hr_timer, ktime_sub(newtime, rtctime), HRTIMER_MODE_REL ); // Start timer
     	printk(KERN_INFO "Timerdev: Timer re-initiated for %d.\n", startpoint_s);
     	timerRunning = true;
     	return 0;
  	} else {
  	    printk(KERN_INFO "Timerdev: Incorrect time spec for reset, not reinitializing.");
  	    return 1;
  	}
}

// Called to set new timer toggle interval values
static int timer_interval_set(char *setting){
    int error_num = 0;
    char *waste;
    char *firstnum;
    char *secnum;
    long int seconds;
    long unsigned int nanoseconds;
    
    if ((waste = strsep(&setting, " ")) == NULL){
        printk(KERN_INFO "Timerdev: Something ain't right with new interval message");
        return 1;
    }
    
    printk(KERN_INFO "Timerdev TEST: Char was %s.", waste);
    
    if ((firstnum = strsep(&setting, " ")) != NULL){
        // Convert firstnum to number
        printk(KERN_INFO "Timerdev TEST: firstnum was %s.", firstnum);
        error_num = kstrtol(firstnum, 10, &seconds);
        if (error_num == 0){
            printk(KERN_INFO "Timerdev: Set new timer interval to %d seconds.", seconds);
        } else {
            printk(KERN_INFO "Timerdev: Could not convert to new time interval %d", error_num);
            return 1;
        }
    } else {
        printk(KERN_INFO "Timerdev: Could not get new time interval from string");
  	    return 1;
    }
    
    if ((secnum = strsep(&setting, " ")) != NULL){
        // Convert secnum to number
        printk(KERN_INFO "Timerdev TEST: secnum was %s.", secnum);
        error_num = kstrtoul(secnum, 10, &nanoseconds);
        if (error_num == 0){
            printk(KERN_INFO "Timerdev: Set new timer interval to %d nanoseconds.", nanoseconds);
        } else {
            printk(KERN_INFO "Timerdev: Could not convert ns to new time interval %d", error_num);
            return 1;
        }
    } else {
        printk(KERN_INFO "Timerdev: Could not get new time ns interval from string");
  	    return 1;
    }
    
    timer_interval_s = seconds;
    timer_interval_ns = nanoseconds;
    
    return 0;
}

// Called to suspend the timer
static void timer_stop(void){
    int ret;
  	ret = hrtimer_cancel( &hr_timer );
  	if (ret) printk("Timerdev: The timer is stopped.\n");
  	timerRunning = false;
}

// Called to manually toggle the pin
static void pin_toggle(void){
    pinOn = !pinOn;
	gpio_set_value(gpioPin, pinOn);
}

module_init(timer_init);
module_exit(timer_exit);
