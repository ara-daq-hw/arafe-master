# -*- coding: utf-8 -*-
#Python script to have serial interface over USB with the ARAFE master
#Author: Brian Clark (clark.2668@osu.edu)
#The lion share of the code is just converting the i2c protocol to something that's friendly for serial ASCII

from cmd import Cmd
import serial
import io
import datetime

ser=serial.Serial()

class Prompt(Cmd):

	def do_serial_init(self,args):
		"""Starts up the serial port. Format is 'serial_init [port]' where the port is format '/dev/ttyUSB0' """
                try:
			start_serial(args)
			print "using serial line %s" %ser.port
		except:
			print "something went wrong with starting the serial port, try again?"

	def do_slave_power(self,args):
		"""Sets the slave power. Format is 'slave_power [slave number: 0-3], [on or off: 0 for off, 1 for on]'"""
		print "WARNING. Using this method of setting the slave power turns off the other three channels."
		arguments = args.split(",") #split the arguments up on the comma
		if len(arguments)!=2:
			print "Wrong number of arguments! Format is 'slave_power [slave number 0-3] [on or off: 0 for off, 1 for on]"
		else:
			slave_setting = [0,0,0,0] #the four slave power registers, in the order [slave 3, slave 2, slave 1, slave 0]
			slave_setting[int(arguments[0])]=int(arguments[1]) #turn this slave on
			binary = '1000%d%d%d%d' %(slave_setting[3],slave_setting[2],slave_setting[1],slave_setting[0]) #make the binary version of the data
			data = hex(int(binary,2)).lstrip('0x') #convert it to hex
			register='00' #add the register in hex
			command = 'c%s%s!' %(register,data) #make the hex command with the right start and stop characters
			ser.write(command) #actually write the command to the serial line
			
			#listen to the output and clear the line
			line = ser.readline()
			while len(line) >0:
				print line.rstrip()
				line = ser.readline()

	def do_allslave_power(self,args):
		"""Sets the slave power of each channel individually, from slave number 0 to number 3. Format is 'allslave_power [channel 0 on or off: 1 or 0], [channel 1 on or off: 1 or 0], [channel 2 on or off: 1 or 0], [channel 3 on or off: 1 or 0]'"""
		arguments = args.split(",") #split the arguments up on the comma
		if len(arguments)!=4:
			print "Wrong number of arguments! Format is 'allslave_power [channel 0 on or off: 1-0], [channel 1 on or off: 1-0], [channel 2 on or off: 1-0], [channel 3 on or off: 1-0]'"
		else:
			slave_setting = [0,0,0,0] #create an array for the four slave power registers, in the order [slave 3, slave 2, slave 1, slave 0]
			slave_setting[0]=int(arguments[0]) #turn slave 0 on/off
			slave_setting[1]=int(arguments[1]) #turn slave 1 on/off
			slave_setting[2]=int(arguments[2]) #turn slave 2 on/off
			slave_setting[3]=int(arguments[3]) #turn slave 3 on/off
			binary = '1000%d%d%d%d' %(slave_setting[3],slave_setting[2],slave_setting[1],slave_setting[0]) #make the binary version of the data
			data = hex(int(binary,2)).lstrip('0x') #convert it to hex
			register='00' #add the register in hex
			command = 'c%s%s!' %(register,data) #make the hex command with the right start and stop characters
			ser.write(command) #actually write the command to the serial line
			
			#listen to the output and clear the line
			line = ser.readline()
			while len(line) >0:
				print line.rstrip()
				line = ser.readline()

	def do_set_atten(self, args):
		"""Sets the signal or trigger of any channel of any slave with a valid setting. Format is 'set_atten [slave: 0-3], [channel: 0-3], [signal or trigger: 0-1], [setting: 0-127]' """
		arguments = args.split(",") #split the arguments up on the comma
		if len(arguments)!=4:
			print "Wrong number of arguments! Format is 'set_atten [slave: 0-3], [channel: 0-3], [signal or trigger: 0-1], [setting: 0-127]'"
		elif 0 > int(arguments[0]) or int(arguments[0]) > 3: #only allow correct slave numbers
			print "Wrong slave number. Slave numbers are 0,1,2, or 3"
		elif 0 > int(arguments[1]) or int(arguments[1]) > 3: #only allow correct channel numbers
			print "Wrong channel number. Channel numbers are 0,1,2, or 3"
		elif 0 != int(arguments[2]) and 1 != int(arguments[2]): #only allow correct sig or trig numbers
			print "Wrong singal or trigger number. The signal or trigger number is 0 or 1 respectively"
		elif 0 > int(arguments[3]) or int(arguments[3]) > 127: #only allow correct setting numbers
			print "Wrong setting number. Setting numbers are whole numbers 0-127"
		else: 
			if arguments[2]==0: #if command is signal
				settingchannel=int(arguments[1]) #copy the channel right over
			else: #if its trigger
				settingchannel=4+int(arguments[1]) #add four to it
			data = '0%d' %(settingchannel)#create the proper command
			command = 'c05%s!' %(data) #create the command
			ser.write(command) #actually write the command to the serial line

			#listen to the output and clear the line
			line = ser.readline()
			while len(line) >0:
				print line.rstrip()
				line = ser.readline()
			
			#okay, now to figure out the setting
			arg = hex(int(arguments[3])).lstrip('0x') #convert it to hex #convert the setting straight to a hex number
			argument = 'c06%s!' %(arg) #creates the proper argument #assign it to a command with its register
			ser.write(argument) #actually write the argument to the serial line

			#listen to the output and clear the line
			line = ser.readline()
			while len(line) >0:
				print line.rstrip()
				line = ser.readline()
		
			#picking the slave
			slaveBinary = "{0:b}".format(int(arguments[0])) #should convert which slave it is to binary
			if int(slaveBinary) == 0 or int(slaveBinary) == 1:
				slaveCtrl = '1000000%s' %(str(slaveBinary))
			else:
				slaveCtrl = '100000%s' %(str(slaveBinary))

			slaveCtrlHex = hex(int(slaveCtrl)).lstrip('0x') #convert it to hex
			slaveControl = 'c05%s!' %(slaveCtrlHex) #creates the proper argument #assign it to a command with its register
			ser.write(slaveControl) #actually write the slaveCtrl to the serial line

			#listen to the output and clear the line
			line = ser.readline()
			while len(line) >0:
				print line.rstrip()
				line = ser.readline()
			
def start_serial(try_port):                                         
	print "got inside start_serial"
	try:
                ser.port=try_port
		#try to open the port
                print ser.port
                ser.open()
                print "Opening USB successful"
		#if opening will fail, it's either going to fail because the port is already open or because we need to switch ports    
	except:
		print "this USB Port cannot be opened, please try running with a different port"
		return

        #first, just assign the port above
        #then, close it, set up the way we want it to talk, and re-open it                                                                 
        ser.close()
        ser.baudrate=9600
        ser.timeout=1
        ser.open()
        ser.setDTR(0)
        print "using serial line %s" %ser.port

        #To be more verbose, we have to verify that the port actually exists before we can do anything to it or else it will fault python
        #The cleanest way to do that is to just open it, but that would initiate it with the wrong baudrate, etc
        #So, first we open it, then close it, then set the baudrates and such, and then finally re-open it
        #See what I mean about dumb?     

if __name__=='__main__':
	prompt = Prompt()
	prompt.prompt='>'
	prompt.cmdloop('Starting command prompt ...')
