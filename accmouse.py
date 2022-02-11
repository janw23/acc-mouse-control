import sys
import math

import numpy as np
import mouse



class AccMouse:
	def __init__(self, buffer_len):
		self.frame_buffer = np.zeros((buffer_len, 3))
		self.counter = 0
		self.stationary = True
		self.stationary_acc = np.zeros(3)
		self.velocity = np.zeros(2)

	def update(self, acc):
		self.counter += 1
		acc = np.array([acc['x'], acc['y'], acc['z']])

		self.frame_buffer = np.roll(self.frame_buffer, 1, axis=0)
		self.frame_buffer[0, :] = acc

		# wait until frame buffer has enough frames
		if self.counter < len(self.frame_buffer):
			return

		# print(self.frame_buffer)

		if self.stationary:
			acc_max = self.frame_buffer[-4:].max(axis=0)
			acc_min = self.frame_buffer[-4:].min(axis=0)

			threshold = 2 / 128
			if ((acc_max - acc_min) > threshold).any():
				self.stationary = False

		else: # mouse is moving
			acc_max = self.frame_buffer.max(axis=0)
			acc_min = self.frame_buffer.min(axis=0)

			threshold = 2 / 128
			if ((acc_max - acc_min) <= threshold).all():
				self.stationary = True

		if self.stationary:
			print('STATIONARY')
		else:
			print('MOVING')


		if self.stationary:
			moving_avg_coef = 0.99
			self.stationary_acc = moving_avg_coef * self.stationary_acc + (1 - moving_avg_coef) * self.frame_buffer[-1]

			self.velocity[:] = 0
		else:
			self.velocity += (self.frame_buffer[-1, :2] - self.stationary_acc[:2]) * 1/50

		mouse.move(self.velocity[1] * -7000, self.velocity[0] * -7000, absolute=False)




if __name__ == '__main__':
	if len(sys.argv) < 2:
		print("Usage: accmouse.py <tty device file>")

	mouse_controller = AccMouse(buffer_len = 5)

	with open(sys.argv[1], 'r') as ttydevice:
		for line in ttydevice:
			line = line.strip().split()

			acc = {'x': int(line[0]), 'y': int(line[1]), 'z': int(line[2])}

			for axis in acc:
				acc[axis] = ((acc[axis] + 127) % 256 - 128) / 128

			print(acc)
			mouse_controller.update(acc)			

			mouse.move(1080 * 0, 720 * 0 + acc['x'], absolute=False)
			
