from PIL import Image, ImageTk
import tkinter as tk
import subprocess
import os
import time
import signal

root = tk.Tk()
proc_args = ['./command']

# Send CTRL + C to subproc if encountered in app
try:
	rec_proc = subprocess.Popen(proc_args, shell=False)
except KeyboardInterrupt:
	rec_proc.send_signal(signal.SIGINT)

##### MAC ATTRIBUTES
#root.wm_attributes("-alpha", 0)

##### WINDOWS ATTRIBUTES
root.overrideredirect(True)
root.wm_attributes("-topmost", True)
root.wm_attributes("-disabled", True)
root.wm_attributes("-transparentcolor", "black")
root.config(bg = "black")

# load images
base = tk.PhotoImage(file="base.gif")
high_prio = tk.PhotoImage(file="high_prio.gif")
low_prio = tk.PhotoImage(file="low_prio.gif")

# overlay image
overlay = tk.Label(root, image=base, bg="black")

root.geometry("150x150")
root.title("Speech Alert")
root.iconphoto(True, base)

# WINDOWS
def change_overlay(overlay, new_image):
	overlay.configure(image=new_image)
	overlay.image = new_image
#

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
			#root.after(2000, lambda: root.iconphoto(True, base))
			# WINDOWS
			root.after(2000, lambda: change_overlay(overlay, base))
			
	def run(self, root, base, high_prio, low_prio):
		while True:
			self.watch(root, base, high_prio, low_prio)
	
# run output monitoring for speech recog probab
fw = FileWatcher()
fw.run(root, base, high_prio, low_prio)

# load overlay
overlay.pack()

# run app
root.mainloop()

