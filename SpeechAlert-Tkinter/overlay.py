from PIL import Image, ImageTk
import tkinter as tk
import subprocess
import os
import time
import signal

root = tk.Tk()
proc_args = ['./command']
rec_proc = subprocess.Popen(proc_args, shell=False)

root.wm_attributes("-alpha", 0)

base = tk.PhotoImage(file="base.gif")
high_prio = tk.PhotoImage(file="high_prio.gif")
low_prio = tk.PhotoImage(file="low_prio.gif")

root.geometry("150x150")
root.title("Speech Alert")
root.iconphoto(True, base)

class FileWatcher:
	def __init__(self):
		self.cached_stamp = 0
		self.filename = "output.txt"
	def watch(self, root, base, high_prio, low_prio):
		self.stamp = os.stat(self.filename).st_mtime
		if self.stamp != self.cached_stamp:
			self.cached_stamp = self.stamp
			self.output_file = open(self.filename, "r")
			self.probab = self.output_file.read()
			if float(self.probab) > 0.3:
				root.iconphoto(True, high_prio)
				time.sleep(2)
				root.iconphoto(True, base)
			else:
				root.iconphoto(True, low_prio)
				time.sleep(2)
				root.iconphoto(True, base)
	def run(self, root, base, high_prio, low_prio):
		while True:
			self.watch(root, base, high_prio, low_prio)
			
fw = FileWatcher()
fw.run(root, base, high_prio, low_prio)

try:
	root.mainloop()
except KeyboardInterrupt:
	rec_proc.kill()

