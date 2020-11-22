import socket
import pyaudio
import threading

class Audio(object):
	def __init__(self, ipServer, portAudio = 5000):
		#Variables
		self.ip_server = ipServer
		self.portAudio = portAudio
		
		#No modificables!!!!
		self.FORMAT = pyaudio.paInt16
		self.CHANNELS = 4
		self.RATE = 16000
		self.CHUNK = 480

		#Feedback
		print("Start Audio")
		print("IP_SERVER {0}:{1}",ipServer,portAudio)

		#Detectar respeacker (microfono)
		self.p = pyaudio.PyAudio()
		self.device_index = None
		print(self.p.get_device_count())
		for i in range(self.p.get_device_count()):
			dev = self.p.get_device_info_by_index(i)
			name = dev['name'].encode('utf-8')
			#print(i, name, dev['maxInputChannels'], dev['maxOutputChannels'])
			if dev['maxInputChannels'] == self.CHANNELS:
				#print('Use {}'.format(name))
				self.device_index = i
				break
		if self.device_index is None:
			print(self.device_index)
			raise Exception('can not find input device with {} channel(s)'.format(self.CHANNELS))
		
		#Iniciando respeacker (microfono)
		self.stream = self.p.open(
			format = self.FORMAT, 
			channels = self.CHANNELS, 
			rate = self.RATE, 
			input = True,
			start = False, 
			frames_per_buffer = self.CHUNK,
			stream_callback=self._callback,
			input_device_index=self.device_index)

		#Iniciando transmision
		self.sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM) #internet # UDP
		print("Initialized Service")

	def _callback(self, in_data, frame_count, time_info, status):
		self.sock.sendto( in_data, (self.ip_server, self.portAudio) )
		return None, pyaudio.paContinue
	
	def start(self):
		self.stream.start_stream()
	
	def stop(self):
		self.sock.close()
		self.stream.stop_stream()
		self.stream.close()
		self.p.terminate()

	
def test_mic():
	import signal
	is_quit = threading.Event()

	ip_server = "192.168.1.128"
	portAudio = 50000

	def signal_handler(sig, num):
		is_quit.set()
		print('Quit')

	signal.signal(signal.SIGINT, signal_handler)

	mic = Audio(ip_server,portAudio)
	mic.start()
	print("-----")
	while True:
		
		if is_quit.is_set():
			mic.stop()
			break
	print("-----")

if __name__ == '__main__':
	test_mic()