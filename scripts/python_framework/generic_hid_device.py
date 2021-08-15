from __future__ import print_function
from mooltipass_defines import *
from datetime import datetime
from array import array
from udelays import *
import platform
import usb.core
import usb.util
import random
import signal
import time
import sys

# Set to true to get advanced debugging information
HID_DEVICE_DEBUG = False

# Packet max payload
HID_PACKET_DATA_PAYLOAD	= 62

# Last message ack flag
LAST_MESSAGE_ACK_FLAG = 0x40


# Generic HID device class
class generic_hid_device:

	# Device constructor
	def __init__(self):
		# Install SIGINT catcher
		signal.signal(signal.SIGINT, self.signal_handler)
		# Set to true to enable ack flag request
		self.ack_flag_in_comms = False

	# Catch CTRL-C interrupt
	def signal_handler(self, signal, frame):
		print("Keyboard Interrupt")
		self.disconnect()
		sys.exit(0)

	# Message to HID packets
	def get_packets_from_message(self, message):
		# Packets array
		packets = []

		# Prepare serialized message
		message_payload = array('B')
		message_payload.extend(message["cmd"])
		message_payload.extend(message["len"])
		message_payload.extend(message["data"])

		# Remaining payload bytes to send
		remaining_bytes = len(message_payload)

		# Compute number of packets
		nb_packets = int(((remaining_bytes + HID_PACKET_DATA_PAYLOAD - 1) / HID_PACKET_DATA_PAYLOAD)) - 1

		# Current packet ID
		cur_packet_id = 0
		cur_byte_index = 0

		# Loop until we've done with all the data
		while cur_packet_id <= nb_packets:
			packet = array('B')

			# compute payload length
			payload_length = HID_PACKET_DATA_PAYLOAD
			if remaining_bytes < HID_PACKET_DATA_PAYLOAD:
				payload_length = remaining_bytes

			# payload length & flip bit
			if self.ack_flag_in_comms:
				packet.append(self.flipbit | LAST_MESSAGE_ACK_FLAG | payload_length)
			else:
				packet.append(self.flipbit | payload_length)

			## packet id
			packet.append((cur_packet_id << 4) | nb_packets)

			# payload
			packet.extend(message_payload[cur_byte_index:cur_byte_index+payload_length])

			# update local vars
			remaining_bytes -= payload_length
			cur_byte_index += payload_length
			cur_packet_id += 1

			# append packet
			packets.append(packet)

		# Flip bit
		if self.flipbit == 0x00:
			self.flipbit = 0x80
		else:
			self.flipbit = 0x00

		# Return packets array
		return packets

	# Send HID packet to device
	def sendHidPacket(self, data):
		# check that we're actually connected to a device
		if self.connected == False:
			print("Not connected to device")
			return

		# debug: print sent data
		if HID_DEVICE_DEBUG:
			print("TX DBG data:", ' '.join(hex(x) for x in data))

		# send data
		self.epout.write(data, 10000)
		delayMicroseconds(700)

	# Send message to device
	def sendHidMessage(self, message):
		# Get packets for message
		packets = self.get_packets_from_message(message)

		# Send all packets
		for packet in packets:
			self.sendHidPacket(packet)
		
	# Send message to device
	def sendHidMessageWaitForAck(self, message, retry_if_retry_received=True):
		# Get packets for message
		packets = self.get_packets_from_message(message)

		# Send all packets
		for packet in packets:
			self.sendHidPacket(packet)

		# Wait for aux MCU ack and main ack
		if self.ack_flag_in_comms:
			self.receiveHidPacket(True)
			
		# Loop
		while True:	
			receive_return = self.receiveHidMessage(True)
			if receive_return == None:
				sys.exit(0)
			elif (receive_return == True and retry_if_retry_received) or (receive_return["cmd"] == CMD_GET_DEVICE_STATUS and (message["cmd"][0] + message["cmd"][1] * 256) != CMD_GET_DEVICE_STATUS):
				print("please retry received... retrying")
				time.sleep(1)
				
				# Get packets for message
				packets = self.get_packets_from_message(message)
		
				# Send all packets again
				for packet in packets:
					self.sendHidPacket(packet)

				# Wait for aux MCU ack and main ack
				if self.ack_flag_in_comms:
					self.receiveHidPacket(True)
			else:
				return receive_return

	# Receive HID packet, crash when nothing is sent
	def receiveHidPacket(self, exit_on_timeout):
		# check that we're actually connected to a device
		if self.connected == False:
			print("Not connected to device")
			return None

		# read from endpoint
		try :
			data = self.epin.read(self.epin.wMaxPacketSize, timeout=self.read_timeout)
			if HID_DEVICE_DEBUG:
				print("RX DBG data:", ' '.join(hex(x) for x in data))
			return data
		except usb.core.USBError as e:
			if HID_DEVICE_DEBUG:
				print(e)
			if exit_on_timeout:
				sys.exit("Device didn't send a packet")
			else:
				return None

	# Receive Message
	def receiveHidMessage(self, exit_on_timeout):
		# For double checks
		nb_payload_bytes_received = 0

		# Create packet object
		packet = {}
		packet["cmd"] = -1
		packet["len"] = -1
		packet["data"] = array('B')

		# Loop until we receive everything
		while True:
			answer = self.receiveHidPacket(exit_on_timeout)			
			if exit_on_timeout == False and answer == None:
				return None
			payload_length = answer[0] & 0x3F
			current_packet = (answer[1] & 0xF0) >> 4
			total_packets = (answer[1] & 0x0F)
			nb_payload_bytes_received += payload_length

			# First packet?
			if packet["cmd"] == -1:
				packet["cmd"] = answer[2] + answer[3] * 256
				packet["len"] = answer[4] + answer[5] * 256
				packet["data"].extend(answer[6:2+payload_length])
			else:
				packet["data"].extend(answer[2:2+payload_length])

			# Check for completion
			if current_packet == total_packets:
				# Check for payload length match
				if len(packet["data"]) != packet["len"] or len(packet["data"]) != nb_payload_bytes_received - 4:
					print("Message receive: error in payload size!")
					print("len(packet) = " + str(len(packet["data"])))
					print("packet[\"len\"] = " + str(packet["len"]))
					print("nb payload bytes received - 4 = " + str(nb_payload_bytes_received - 4))
					return None
				else:
					# Check for debug message
					if packet["cmd"] == CMD_DBG_MESSAGE:
						temp_string = u"DBG: ";
						for i in range(0, int(packet["len"]/2)):
							if sys.version_info[0] < 3:
								temp_string += unichr(packet["data"][i*2] + packet["data"][i*2+1]*256)
							else:
								temp_string += chr(packet["data"][i*2] + packet["data"][i*2+1]*256)
						print(temp_string)
					elif packet["cmd"] == CMD_ID_RETRY:
						return True
					return packet

	# Receive HID packet, return None when nothing is sent
	def receiveHidPacketWithTimeout(self):
		try :
			data = self.epin.read(self.epin.wMaxPacketSize, timeout=self.read_timeout)
			if HID_DEVICE_DEBUG:
				print("RX DBG data:", ' '.join(hex(x) for x in data))
			return data
		except usb.core.USBError as e:
			return None

	# Set new read timeout
	def setReadTimeout(self, read_timeout):
		self.read_timeout = read_timeout

	# Try to connect to HID device.
	# ping_packet: an array containing a ping packet to send to the device over HID
	def connect(self, print_debug, device_vid, device_pid, read_timeout, ping_packet):
		# Reset vars
		self.connected = False
		self.flipbit = 0x00
		
		# Find our device
		self.hid_device = usb.core.find(idVendor=device_vid, idProduct=device_pid)
		self.read_timeout = read_timeout

		# Generate our hid message from our ping message (cheating: we're only doing one HID packet)
		hid_packet = self.get_packets_from_message(ping_packet)[0]

		# Force aux MCU ACK request
		hid_packet[0] |= LAST_MESSAGE_ACK_FLAG

		# Was it found?
		if self.hid_device is None:
			if print_debug:
				print("Device not found")
			return False

		# Device found
		if print_debug:
			print("USB device found")

		# Different init codes depending on the platform
		if platform.system() == "Linux":
			# Need to do things differently
			try:
				self.hid_device.detach_kernel_driver(0)
				self.hid_device.reset()
			except Exception as e:
				pass # Probably already detached
		else:
			# Set the active configuration. With no arguments, the first configuration will be the active one
			try:
				self.hid_device.set_configuration()
			except Exception as e:
				if print_debug:
					print("Cannot set configuration the device:" , str(e))
				return False

		if HID_DEVICE_DEBUG:
			for cfg in self.hid_device:
				print("configuration val:", str(cfg.bConfigurationValue))
				for intf in cfg:
					print("int num:", str(intf.bInterfaceNumber), ", int alt:", str(intf.bAlternateSetting))
					for ep in intf:
						print("endpoint addr:", str(ep.bEndpointAddress))

		# Get an endpoint instance
		cfg = self.hid_device.get_active_configuration()
		intf = cfg[(0,0)]

		# Match the first OUT endpoint
		self.epout = usb.util.find_descriptor(intf, custom_match = lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT)
		if self.epout is None:
			self.hid_device.reset()
			return False

		if HID_DEVICE_DEBUG:
			print("Selected OUT endpoint:", self.epout.bEndpointAddress)

		# Match the first IN endpoint
		self.epin = usb.util.find_descriptor(intf, custom_match = lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_IN)
		if self.epin is None:
			self.hid_device.reset()
			return False

		if HID_DEVICE_DEBUG:
			print("Selected IN endpoint:", self.epin.bEndpointAddress)

		time.sleep(0.5)
		
		# Set flip bit reset packet
		flipbit_reset_packet = array('B')
		flipbit_reset_packet.append(0xFF)
		flipbit_reset_packet.append(0xFF)
		first_packet_send_counter = 0
		first_packet_not_sent = True
		while first_packet_not_sent and first_packet_send_counter < 10:
			try:
				self.epout.write(flipbit_reset_packet)
				first_packet_not_sent = False
			except:
				first_packet_send_counter+=1
				time.sleep(1)
		if first_packet_not_sent:
			return False
		
		try:
			# try to send ping packet
			self.epout.write(hid_packet)
			if HID_DEVICE_DEBUG:
				print("TX DBG data:", ' '.join(hex(x) for x in hid_packet))
			# try to receive one answer
			temp_bool = True
			while temp_bool:
				try :
					# try to receive aux MCU answer
					data = self.epin.read(self.epin.wMaxPacketSize, timeout=500)
					if HID_DEVICE_DEBUG:
						print("RX DBG data:", ' '.join(hex(x) for x in data))
					# check that the received data is correct (cheating as we know we should receive one packet)
					if hid_packet == data[0:len(hid_packet)]:
						if print_debug:
							print("Received aux MCU ACK")
					else:
						if print_debug:
							print("Incorrect Aux MCU ACK...")
							print("Cleaning remaining input packets")
						continue
					# try to receive answer
					data = self.epin.read(self.epin.wMaxPacketSize, timeout=500)
					if HID_DEVICE_DEBUG:
						print("RX2 DBG data:", ' '.join(hex(x) for x in data))
					# check that the received data is correct (cheating as we know we should receive one packet)
					# remove useless ACK
					hid_packet[0] = hid_packet[0] & ~LAST_MESSAGE_ACK_FLAG
					if hid_packet == data[0:len(hid_packet)]:
						if print_debug:
							print("Received main MCU ACK")
						temp_bool = False
					else:
						if print_debug:
							print("Incorrect main MCU ping answer...")
							print("Cleaning remaining input packets")
						continue
					time.sleep(.5)
				except usb.core.USBError as e:
					self.hid_device.reset()
					if print_debug:
						print(e)
					return False
		except usb.core.USBError as e:
			if print_debug:
				print(e)
			return False

		# Set connected var, return success
		self.connected = True
		return True

	# Disconnect from HID device
	def disconnect(self):
		# check that we're actually connected to a device
		if self.connected == False or self.hid_device == None:
			print("Not connected to device")
			return
		else:
			print("Disconnecting from device...")

		# reset device
		usb.util.dispose_resources(self.hid_device)
		self.hid_device.reset()

	# Benchmark ping pong speed
	def benchmarkPingPongSpeed(self, ping_packet):
		# check that we're actually connected to a device
		if self.connected == False:
			print("Not connected to device")
			return

		# Generate our hid message from our ping message (cheating: we're only doing one HID packet)
		hid_packet = self.get_packets_from_message(ping_packet)[0]

		# start ping ponging
		current_second = datetime.now().second
		data_counter = 0
		while True:
			self.sendHidPacket(hid_packet)
			if self.ack_flag_in_comms:
				self.receiveHidPacket(True)
			self.receiveHidPacket(True)
			data_counter += 64

			# Print out performance
			if current_second != datetime.now().second:
				current_second = datetime.now().second
				print("Ping pong transfer speed (unidirectional):", data_counter , "B/s")
				if self.ack_flag_in_comms:
					print("Ping pong transfer speed (bidirectional cumulated):", data_counter*3 , "B/s")
				else:
					print("Ping pong transfer speed (bidirectional cumulated):", data_counter*2 , "B/s")
				data_counter = 0
