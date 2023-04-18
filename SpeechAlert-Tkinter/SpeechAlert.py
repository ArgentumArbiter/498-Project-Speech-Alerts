from PIL import Image, ImageTk
import tkinter as tk
import subprocess
import os
import time
import signal
import threading
import re

# WINDOWS
def change_overlay(overlay, new_image):
	overlay.configure(image=new_image)
	overlay.image = new_image
#

class App:
	def __init__(self):
		##### MAC ATTRIBUTES
		#root.wm_attributes("-alpha", 0)

		##### WINDOWS ATTRIBUTES
		root.overrideredirect(True)
		root.wm_attributes("-topmost", True)
		root.wm_attributes("-transparentcolor", "black")
		root.config(bg = "black")
		
		screen_height = root.winfo_screenheight() - 250
		
		root.geometry("+0+%d" % (screen_height))
		root.title("Speech Alert")
		root.iconphoto(True, base)
		root.protocol("WM_DELETE_WINDOW", self.close_app)

		# run output monitoring for speech recog probab
		fw = FileWatcher()
		#fw.run(root, base, high_prio, low_prio)
		threading.Thread(target=fw.run, args=(root, base, high_prio, low_prio)).start()
		
		threading.Thread(target=self.run_rec_proc).start()
		
		root.mainloop()
	def close_app(self):
		os.kill(self.rec_proc.pid, signal.CTRL_C_EVENT)
		root.destroy()

	def run_rec_proc(self):
		# Send CTRL + C to subproc if encountered in app
		proc_args = ['./command']
		startupinfo = subprocess.STARTUPINFO()
		startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
		self.rec_proc = subprocess.Popen(proc_args, shell=False, startupinfo=startupinfo)
		

class FileWatcher:
	def __init__(self):
		self.cached_stamp = 0
		self.filename = "output.txt"
	def watch(self, root, base, high_prio, low_prio):
		self.stamp = os.stat(self.filename).st_mtime
		if self.stamp != self.cached_stamp:
			self.cached_stamp = self.stamp
			time.sleep(0.1)
			with open(self.filename, "r") as output_file:
				output_file.seek(0)
				self.probab = output_file.read()
			if float(self.probab) > 0.1:
				# MAC
				#root.iconphoto(True, high_prio)
				# WINDOWS
				change_overlay(overlay, high_prio)
			else:
				# MAC
				#root.iconphoto(True, low_prio)
				# WINDOWS
				change_overlay(overlay, low_prio)
			# MAC
			#root.after(5000, lambda: root.iconphoto(True, base))
			# WINDOWS
			root.after(5000, lambda: change_overlay(overlay, base))
	def run(self, root, base, high_prio, low_prio):
		while True:
			time.sleep(1)
			self.watch(root, base, high_prio, low_prio)

root = tk.Tk()

# load images
base = tk.PhotoImage(file="base.gif")
high_prio = tk.PhotoImage(file="high_prio.gif")
low_prio = tk.PhotoImage(file="low_prio.gif")

# overlay image
overlay = tk.Label(root, image=base, bg="black")

# load overlay
overlay.pack()
		
app = App()




