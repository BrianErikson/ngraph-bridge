# Copyright 2015 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
"""A simplified deep MNIST classifier using convolutional layers.
This script has the following changes when compared to mnist_deep.py:
1. no dropout layer (which disables the rng op)
2. no truncated normal initialzation(which disables the while op)

See extensive documentation at
https://www.tensorflow.org/get_started/mnist/pros
"""
# Disable linter warnings to maintain consistency with tutorial.
# pylint: disable=invalid-name
# pylint: disable=g-bad-import-order

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import sys
import tempfile
import getpass
import time

from tensorflow.examples.tutorials.mnist import input_data

import tensorflow as tf
import ngraph_bridge

FLAGS = None


def deepnn(x):
    """deepnn builds the graph for a deep net for classifying digits.

  Args:
    x: an input tensor with the dimensions (N_examples, 784), where 784 is the
    number of pixels in a standard MNIST image.

  Returns:
    A tuple (y, a scalar placeholder). y is a tensor of shape (N_examples, 10), with values
    equal to the logits of classifying the digit into one of 10 classes (the
    digits 0-9). The scalar placeholder is meant for the probability of dropout. Since we don't
    use a dropout layer in this script, this placeholder is of no relavance and acts as a dummy.
  """
    # Fully connected Layer 1 
    with tf.name_scope('fc1'):
        W_fc1 = weight_variable([784, 10], "Weight_fc1")
        b_fc1 = bias_variable([10], "Bias_fc1")

        #h_pool1_flat = tf.reshape(h_pool1, [-1, 14 * 14 * 32])
        y_conv = tf.matmul(x, W_fc1) + b_fc1

    return y_conv, tf.placeholder(tf.float32)


def weight_variable(shape, name):
    """weight_variable generates a weight variable of a given shape."""
    initializer = tf.constant(0.1, shape=shape)
    initial = tf.get_variable(name, initializer=initializer)
    #initial = tf.constant(0.1, shape=shape)
    return initial


def bias_variable(shape, name):
    """bias_variable generates a bias variable of a given shape."""
    initial = tf.constant(0.1, shape=shape)
    return tf.Variable(initial,name=name)


def train_mnist_cnn(FLAGS):
    # Config
    config = tf.ConfigProto(
        allow_soft_placement=True,
        log_device_placement=False,
        inter_op_parallelism_threads=1)

    # Note: Additional configuration option to boost performance is to set the
    # following environment for the run:
    # OMP_NUM_THREADS=44 KMP_AFFINITY=granularity=fine,scatter
    # The OMP_NUM_THREADS number should correspond to the number of
    # cores in the system

    # Import data
    mnist = input_data.read_data_sets(FLAGS.data_dir, one_hot=True)

    # Create the model
    x = tf.placeholder(tf.float32, [None, 784])

    # Define loss and optimizer
    y_ = tf.placeholder(tf.float32, [None, 10])

    # Build the graph for the deep net
    y_conv, keep_prob = deepnn(x)

    with tf.name_scope('loss'):
        cross_entropy = tf.nn.softmax_cross_entropy_with_logits(
            labels=y_, logits=y_conv)
    cross_entropy = tf.reduce_mean(cross_entropy)

    with tf.name_scope('gradient_Descent'):
        train_step = tf.train.GradientDescentOptimizer(1e-2).minimize(cross_entropy)

    with tf.name_scope('accuracy'):
        correct_prediction = tf.equal(tf.argmax(y_conv, 1), tf.argmax(y_, 1))
        correct_prediction = tf.cast(correct_prediction, tf.float32)
    accuracy = tf.reduce_mean(correct_prediction)
    tf.summary.scalar('Training accuracy', accuracy)
    tf.summary.scalar('Loss function', cross_entropy)

    graph_location = "/tmp/" + getpass.getuser(
    ) + "/tensorboard-logs/mnist-convnet"
    print('Saving graph to: %s' % graph_location)

    merged = tf.summary.merge_all()
    train_writer = tf.summary.FileWriter(graph_location)
    train_writer.add_graph(tf.get_default_graph())

    saver = tf.train.Saver()

    with tf.Session(config=config) as sess:
        sess.run(tf.global_variables_initializer())
        train_loops = FLAGS.train_loop_count
        loss_values = []
        for i in range(train_loops):
            batch = mnist.train.next_batch(FLAGS.batch_size, shuffle=False)
            if i % 10 == 0:
                t = time.time()
                train_accuracy = accuracy.eval(feed_dict={
                    x: batch[0],
                    y_: batch[1],
                    keep_prob: 1.0
                })
                tf.summary.scalar('Training accuracy', train_accuracy)
                print('step %d, training accuracy %g, %g sec to evaluate' %
                      (i, train_accuracy, time.time() - t))
                # saver.save(sess, FLAGS.model_dir + "model.ckpt")
            t = time.time()
            _, summary, loss = sess.run([train_step, merged, cross_entropy],
                                        feed_dict={
                                            x: batch[0],
                                            y_: batch[1],
                                            keep_prob: 0.5
                                        })
            loss_values.append(loss)
            print('step %d, loss %g, %g sec for training step' %
                  (i, loss, time.time() - t))
            # train_writer.add_summary(summary, i)

        print("Training finished. Running test")

        num_test_images = FLAGS.test_image_count
        x_test = mnist.test.images[:num_test_images]
        y_test = mnist.test.labels[:num_test_images]

        test_accuracy = accuracy.eval(feed_dict={
            x: x_test,
            y_: y_test,
            keep_prob: 1.0
        })
        print('test accuracy %g' % test_accuracy)
        print('saving models')
        
        return loss_values, test_accuracy


def main(_):
    train_mnist_cnn(FLAGS)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--data_dir',
        type=str,
        default='/tmp/tensorflow/mnist/input_data',
        help='Directory where input data is stored')

    parser.add_argument(
        '--train_loop_count',
        type=int,
        default=100,
        help='Number of training iterations')

    parser.add_argument('--batch_size', type=int, default=50, help='Batch Size')

    parser.add_argument(
        '--test_image_count',
        type=int,
        default=None,
        help="Number of test images to evaluate on")

    parser.add_argument(
        '--model_dir',
        type=str,
        default='./mnist_trained/',
        help='enter model dir')

    FLAGS, unparsed = parser.parse_known_args()
    tf.app.run(main=main, argv=[sys.argv[0]] + unparsed)