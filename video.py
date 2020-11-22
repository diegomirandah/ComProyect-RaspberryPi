
import threading
import subprocess
import time
import signal

class Video(object):
	def __init__(self, ipServer, port1=5001, port2=5002, port3=5003, port4=5004, dv1=0, dv2=2, dv3=4, dv4=6, fmt="v4l2", pxl_fmt="mjpeg", video_size="320x240", out_fmt="mpegts", rate=1):
		#variables
		self.ip_server = ipServer
		self.port1 = port1
		self.port2 = port2
		self.port3 = port3
		self.port4 = port4
		self.input_video = ["/dev/video" + str(dv1),"/dev/video" + str(dv2),"/dev/video" + str(dv3),"/dev/video" + str(dv4)]
		#variables de sistema
		self.comand = ["./c/videoStreaming","--ip",str(self.ip_server),"--dev1", self.input_video[0],"--dev2", self.input_video[1],"--dev3", self.input_video[2],"--dev4", self.input_video[3],"--f"]
		self.sbp = None
		s = " "
		print(s.join(self.comand))

	def start(self):
		self.sbp = subprocess.Popen(self.comand,stdin=subprocess.PIPE, stderr=subprocess.STDOUT)
		
	def stop(self):
		self.sbp.send_signal(signal.SIGINT)

def test_cam():
	import signal
	is_quit = threading.Event()

	ip_server = "192.168.1.128"
	port1 = 5001
	port2 = 5002
	port3 = 5003
	port4 = 5004

	def signal_handler(sig, num):
		is_quit.set()
		print('Quit')
	
	signal.signal(signal.SIGINT, signal_handler)

	vid = Video(ip_server,port1,port2,port3,port4)
	vid.start()
	print("-----")
	while True:
	 	if is_quit.is_set():
	 		vid.stop()
	 		break
	print("-----")


if __name__ == '__main__':
	test_cam()