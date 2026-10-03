#define	KEYBOARD_MONITOR_INPUT_FIFO_NAME		"KeyboardMonitorInputFifo"
int our_input_fifo_filestream = -1;
#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <sstream> 
#include <string.h>

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)

//*************************************************
//*************************************************
//********** KEYBOARD MONITOR INITIALISE **********
//*************************************************
//*************************************************
//Call once at startup
void KeyboardMonitorInitialise (void)
{
	//--------------------------------------
	//----- CREATE A FIFO / NAMED PIPE -----
	//--------------------------------------
	int result;

	printf("Making KeyboardMonitor FIFO...\n");
	result = mkfifo(KEYBOARD_MONITOR_INPUT_FIFO_NAME, 0777);		//(This will fail if the fifo already exists in the system from the app previously running, this is fine)
	if (result == 0)
	{
		//FIFO CREATED
		printf("New FIFO created: %s\n", KEYBOARD_MONITOR_INPUT_FIFO_NAME);
	}

	printf("Process %d opening FIFO %s\n", getpid(), KEYBOARD_MONITOR_INPUT_FIFO_NAME);
	our_input_fifo_filestream = open(KEYBOARD_MONITOR_INPUT_FIFO_NAME, (O_RDONLY | O_NONBLOCK));
					//Possible flags:
					//	O_RDONLY - Open for reading only.
					//	O_WRONLY - Open for writing only.
					//	O_NDELAY / O_NONBLOCK (same function) - Enables nonblocking mode. When set read requests on the file can return immediately with a failure status
					//											if there is no input immediately available (instead of blocking). Likewise, write requests can also return
					//											immediately with a failure status if the output can't be written immediately.
	if (our_input_fifo_filestream != -1)
		printf("Opened input FIFO: %i\n", our_input_fifo_filestream);


	//-------------------------------------------------------
	//----- RUN KEYBOARD CAPTURE ON A BACKGROUND THREAD -----
	//-------------------------------------------------------
	int pid2 = fork();
	if(pid2 == 0)
	{
		//----- THIS IS THE CHILD THREAD -----
		printf("KeyboardMonitor child thread started");

		int FileDevice;
		int ReadDevice;
		int Index;
		struct input_event InputEvent[64];
		int version;
		unsigned short id[4];
		unsigned long bit[EV_MAX][NBITS(KEY_MAX)];
		int NoError = 1;
		stringstream ss1;

		//----- OPEN THE INPUT DEVICE -----
		if ((FileDevice = open("/dev/input/event0", O_RDONLY)) < 0)		//<<<<SET THE INPUT DEVICE PATH HERE
		{
			perror("KeyboardMonitor can't open input device");
			NoError = 0;
		}

		//----- GET DEVICE VERSION -----
		if (ioctl(FileDevice, EVIOCGVERSION, &version))
		{
			perror("KeyboardMonitor can't get version");
			NoError = 0;
		}
		//printf("Input driver version is %d.%d.%d\n", version >> 16, (version >> 8) & 0xff, version & 0xff);

		//----- GET DEVICE INFO -----
		if (NoError)
		{
			ioctl(FileDevice, EVIOCGID, id);
			//printf("Input device ID: bus 0x%x vendor 0x%x product 0x%x version 0x%x\n", id[ID_BUS], id[ID_VENDOR], id[ID_PRODUCT], id[ID_VERSION]);

			memset(bit, 0, sizeof(bit));
			ioctl(FileDevice, EVIOCGBIT(0, EV_MAX), bit[0]);
		}
		
		printf("KeyboardMonitor child thread running"):

		//Create an output filestream for this child thread
		int our_output_fifo_filestream = -1;
		our_output_fifo_filestream = open(KEYBOARD_MONITOR_INPUT_FIFO_NAME, (O_WRONLY | O_NONBLOCK));
							//Possible flags:
							//	O_RDONLY - Open for reading only.
							//	O_WRONLY - Open for writing only.
							//	O_NDELAY / O_NONBLOCK (same function) - Enables nonblocking mode. When set read requests on the file can return immediately with a failure status
							//											if there is no input immediately available (instead of blocking). Likewise, write requests can also return
							//											immediately with a failure status if the output can't be written immediately.
		if (our_output_fifo_filestream != -1)
			cout << "Opened output FIFO: " << our_output_fifo_filestream << endl;
		
		//----- READ KEYBOARD EVENTS -----
		while (NoError)
		{
			ReadDevice = read(FileDevice, InputEvent, sizeof(struct input_event) * 64);

			if (ReadDevice < (int) sizeof(struct input_event))
			{
				//This should never happen
				//perror("KeyboardMonitor error reading - keyboard lost?");
				NoError = 0;
			}
			else
			{
				for (Index = 0; Index < ReadDevice / sizeof(struct input_event); Index++)
				{
					//We have:
					//	InputEvent[Index].time		timeval: 16 bytes (8 bytes for seconds, 8 bytes for microseconds)
					//	InputEvent[Index].type		See input-event-codes.h
					//	InputEvent[Index].code		See input-event-codes.h
					//	InputEvent[Index].value		01 for keypress, 00 for release, 02 for autorepeat

					if (InputEvent[Index].type == EV_KEY)
					{
						ss1.clear();
						ss1.str(std::string());
						
						if (InputEvent[Index].value == 2)
						{
							//This is an auto repeat of a held down key
							//ss1 << (int)(InputEvent[Index].code) << " Auto Repeat";
						}
						else if (InputEvent[Index].value == 1)
						{
							//----- KEY DOWN -----
							ss1 << (int)(InputEvent[Index].code) << " Key Down";	//input-event-codes.h
						}
						else if (InputEvent[Index].value == 0)
						{
							//----- KEY UP -----
							ss1 << (int)(InputEvent[Index].code) << " Key Up";		//input-event-codes.h
						}
						
						//----- WRITE EVENT TO THE OUTPUT FIFO -----
						write(our_output_fifo_filestream, (void*)ss1.str().c_str(), strlen(ss1.str().c_str()));
						
					}
				}
			}
		} 
		close(FileDevice);
		cout << "KeyboardMonitor child thread ending!" << endl;
		_exit(0);		//Don't forget to exit!
	}

}

//**********************************************
//**********************************************
//********** KEYBOARD MONITOR PROCESS **********
//**********************************************
//**********************************************
//Call from main loop
void KeyboardMonitorProcess (void)
{
	//---------------------------------------------
	//----- CHECK FIFO FOR ANY RECEIVED BYTES -----
	//---------------------------------------------
	// Read up to 255 characters from the port if they are there
	if (our_input_fifo_filestream != -1)
	{
		unsigned char InputBuffer[256];
		int BufferLength = read(our_input_fifo_filestream, (void*)InputBuffer, 255);		//Filestream, buffer to store in, number of bytes to read (max)
		if (BufferLength < 0)
		{
			//An error occured (this can happen)
		}
		else if (BufferLength == 0)
		{
			//No data waiting
		}
		else
		{
			//Bytes received
			InputBuffer[BufferLength] = '\0';
			cout << InputBuffer << endl;
		}
	}
}