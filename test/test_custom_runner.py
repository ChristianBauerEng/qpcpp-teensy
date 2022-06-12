from cmath import e
from logging import captureWarnings
from os import popen
from platformio.public import TestRunnerBase
import click
import string
from subprocess import Popen, run, PIPE, CREATE_NEW_CONSOLE
import socket
import threading
from time import sleep
import sys
import os
import glob

class CustomTestRunner(TestRunnerBase):

    def stage_uploading(self):

        # launch a QSPY process that is opened in a dedicated shell
        click.secho("Opening QSPY session...")
        self.qspy_process = Popen('qspy', creationflags=CREATE_NEW_CONSOLE)

        click.echo("TestRunner connecting to qspy on port 6601")
        self.qspy_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.qspy_socket.connect(('localhost', 6601))
        return super().stage_uploading()


    def start_qutest(self):
        # Build list of test-scripts to execute for qutest.
        test_dir = self.project_config.get("platformio", "test_dir")
        test_name = self.test_suite.test_name

        # find python files starting with "test_" in the directory of the current test.
        files = glob.glob(os.path.join(test_dir, test_name, "test_*.py"))
        click.secho("Found the following test files: {}".format(files), fg='green')
        fileListString = ' '.join(files)

        # Find test scripts, run qutest and connect to the qspy process on the default port.  
        thisPath = os.path.dirname(os.path.realpath(__file__))
        qutest_path = os.path.join(thisPath, 'qutest.py') 
     
        click.secho("Running qutest: {}".format(qutest_path))
        # qutest will write its output to PIPE, that we can read periodically.
        # However, since the reading is blocking, we will delegate that to another thread, from where the output
        # will be processed.
        self.qutest_process = Popen(['python', '-u', qutest_path, fileListString], stdout=PIPE, stderr=PIPE, creationflags=CREATE_NEW_CONSOLE, shell=True, universal_newlines=True)

        # # Start polling
        self.qutest_polling_thread = threading.Thread(group=None, name="qutestPolling", target=self.qutest_polling_loop)
        self.qutest_polling_thread.start()

        # # wait for polling thread to end, which happens when qutest has stopped and all outputs have been processed.
        #self.qutest_polling_thread.join()

        #click.echo("Polling-thread rejoined. Ending unit tests.")

        # TODO: Kill qspy-process and close console

        #self.test_suite.on_finish()


    def stage_testing(self):
        # Pausing for a couple of seconds to let Teensy re-establish Serial connection.
        # sleep(10)
        self.isTesting = False
        
        return super().stage_testing()


    # Run this function in a dedicated thread to continously poll output from the qutest-process
    # Each received line should correspond to a test result.
    def qutest_polling_loop(self):
        click.secho("Starting polling of qutest...", fg='green')
        # Poll qutest output while the process is still alive.
        while self.qutest_process.poll() is None:
            click.echo("Polling qutest...")
            
            # Read all lines in STDOUT
            self.qutest_process.stdout.flush()
            #self.qutest_process.stderr.flush()            
            outputs = self.qutest_process.stdout.read()
            #errors = self.qutest_process.stderr.read()

            click.echo(outputs)
            #click.secho(errors, fg='red')

            # Analyse each line individually.
            # for line in outputs:
            #     click.echo(line)
            #     # TODO: parse lines and add test-cases to the test-suite.

            # Wait for new data
            sleep(0.05)

        out, err = self.qutest_process.communicate()
        click.echo("qutest seems to have stopped. No longer polling")
        click.echo(out)
        click.secho(err, fg="red")
        self.test_suite.on_finish()
        return

    # Processed data received from MCU via debug port.
    # Forwards the received data to the qspy-process.
    def on_testing_data_output(self, data):
        click.secho("D: {}".format(data), fg='yellow')
        self.qspy_socket.sendall(data)

        if not self.isTesting:
            self.start_qutest()
            self.isTesting = True
        return super().on_testing_data_output(data)

    # Called when receiving a complete line from the MCU.
    # Since qspy-produces binary data we can ignore this.
    def on_testing_line_output(self, line):
        click.secho("L: {}".format(line), fg='yellow')
        self.qspy_socket.send(bytes(line, "utf-8"))
        return
    
    def teardown(self):
        click.echo("tearing down tests...")
        # self.qspy_socket.close()
        # self.qspy_process.terminate()
        return super().teardown()