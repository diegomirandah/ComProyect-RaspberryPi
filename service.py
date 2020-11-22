from gpiozero import Button
import signal
import time
from audio import Audio
from video import Video

btn = Button(12)

class Service(object):
	def __init__(self, ipServer, portAudio = 5000, port1 = 5001, port2 = 5002, port3 = 5003, port4 = 5004):
		self.ip_server = ipServer
		self.portAudio = portAudio
		self.port1 = port1
		self.port2 = port2
		self.port3 = port3
		self.port4 = port4
		print("starting service")
		self.mic = Audio(self.ip_server, self.portAudio )
		self.vid = Video(self.ip_server, self.port1, self.port2, self.port3, self.port4)

	def start(self):
		self.mic.start()
		print("Audio service started")
		time.sleep(4)
		self.vid.start()
		print("Video service started")
	
	def stop(self):
		self.vid.stop()
		self.mic.stop()
		print("stopped services")

def wait_button():
	ip_server = "192.168.1.128"
	portAudio = 5000
	port1 = 5001
	port2 = 5002
	port3 = 5003
	port4 = 5004

	print("Waiting button")
	btn.wait_for_release()
	time.sleep(1)
	service = Service(ip_server,portAudio,port1,port2,port3,port4)
	service.start()
	print("Waiting button")
	while True:
		if not btn.is_pressed:
			time.sleep(20)
			service.stop()

if __name__ == '__main__':
	wait_button()