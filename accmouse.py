import sys
import math
from time import perf_counter

import numpy as np
import mouse



class AccMouse:
	def __init__(self, buffer_len):
		self.frame_buffer = np.zeros((buffer_len, 3))
		self.counter = 0
		self.stationary = True
		self.stationary_counter = 0
		self.stationary_acc = np.zeros(3)
		self.velocity = np.zeros(2)
		self.prev_capture_time = 0

	def update(self, acc, capture_time):
		self.counter += 1
		time_delta = capture_time - self.prev_capture_time
		self.prev_capture_time = capture_time

		acc = np.array([acc['x'], acc['y'], acc['z']])

		self.frame_buffer = np.roll(self.frame_buffer, 1, axis=0)
		self.frame_buffer[0, :] = acc

		# wait until frame buffer has enough frames
		if self.counter < len(self.frame_buffer):
			return

		# print(self.frame_buffer)

		if self.stationary:
			acc_max = self.frame_buffer[-3:].max(axis=0)
			acc_min = self.frame_buffer[-3:].min(axis=0)

			threshold = 1 / 128
			if ((acc_max - acc_min) > threshold).any():
				self.stationary = False

		else: # mouse is moving
			acc_max = self.frame_buffer.max(axis=0)
			acc_min = self.frame_buffer.min(axis=0)

			threshold = 1 / 128
			if ((acc_max - acc_min) <= threshold).all():
				self.stationary = True

		if self.stationary:
			print('STATIONARY')
		else:	
			print('MOVING')


		if self.stationary:
			self.stationary_counter += 1
			if self.stationary_counter > 20:
				moving_avg_coef = 0.99
				self.stationary_acc = moving_avg_coef * self.stationary_acc + (1 - moving_avg_coef) * self.frame_buffer[-1]

			self.velocity[:] = 0
		else:
			self.stationary_counter = 0
			self.velocity += (self.frame_buffer[-1, :2] - self.stationary_acc[:2]) * time_delta

		mouse.move(self.velocity[1] * -7000, self.velocity[0] * -7000, absolute=False)




if __name__ == '__main__':
	if len(sys.argv) < 2:
		print("Usage: accmouse.py <tty device file>")

	mouse_controller = AccMouse(buffer_len = 5)

	with open(sys.argv[1], 'rb') as ttydevice:
		for line in ttydevice:
			capture_time = perf_counter()

			# Omit incomplete reads.
			if len(line) != 4:
				continue

			print('len:', len(line), 'line: ', line)
			acc = {'x': line[0], 'y': line[1], 'z': line[2]}
			
			for axis in acc:
				acc[axis] = ((acc[axis] - 32) % 256 - 128) / 128


			print(acc)
			mouse_controller.update(acc, capture_time)			

			mouse.move(1080 * 0, 720 * 0 + acc['x'], absolute=False)
			
