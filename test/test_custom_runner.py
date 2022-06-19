from ast import Try
from cmath import e
from logging import captureWarnings
from os import popen
from platformio.public import TestRunnerBase
from platformio.device.finder import find_serial_port
import serial
import click
import string
from subprocess import Popen, run, PIPE, CREATE_NEW_CONSOLE
import socket
import threading
from time import sleep
import time
import sys
import os
import glob

class CustomTestRunner(TestRunnerBase):

    def stage_uploading(self):
        return super().stage_uploading()


    def start_qspy(self):
        # launch a QSPY process that is opened in a dedicated shell
        click.secho("Opening QSPY session...")
        # self.qspy_process = Popen('qspy', creationflags=CREATE_NEW_CONSOLE)
        self.qspy_process = Popen('qspy', creationflags=CREATE_NEW_CONSOLE)

        click.echo("TestRunner connecting to qspy on port 6601")
        self.qspy_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.qspy_socket.connect(('localhost', 6601))


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


    def send_to_MCU(self, bytes):
        try:
            self.serial.write(bytes)
            self.serial.flush()
        except Exception as exc:
            click.secho("Error sending data to MCU: {}".format(exc))

    def run_Serial(self):

        # Run the serial port as long as the test runner is testing...
        while self.isTesting:

            project_options = self.project_config.items(
                env=self.test_suite.env_name, as_dict=True
            )

            port = find_serial_port(
                initial_port=self.get_test_port(),
                board_config = self.platform.board_config(project_options["board"]),
                upload_protocol=project_options.get("upload_protocol"),
                ensure_ready=False)

            # try to (re)open the serial port. It should be closed after uploading the program.
            startTime = time.perf_counter()
            TIMEOUT = 10
            while(time.perf_counter() - startTime <= TIMEOUT):
                try:
                    click.secho("Attempting to open serial port: {}".format(port))
                    self.serial = serial.Serial(port, 9600, parity=serial.PARITY_NONE, timeout = 5)
                    click.secho("{} opened...".format(port))
                    break
                except Exception as e:
                    click.secho("Could not access serial port: {}".format(port), fg='red')
                    sleep(1)

            # if serial could not be opened withit timeout, give up.
            if(self.serial is None):
                click.secho("Giving up.", fg='red')
                return False

            # Continously read serial output and send it to ourself.
            try:
                while(self.isTesting):
                    self.on_testing_data_output(self.serial.read(self.serial.in_waiting or 1))
                    sleep(0.1)
            except Exception as exc:
                click.secho("Exception during serial monitoring: {}".format(exc))
                self.serial = None



    def stage_testing(self):
        # Pausing for a couple of seconds to let Teensy re-establish Serial connection.
        sleep(2)
        self.isTesting = True

        # self.serialThread = threading.Thread(target=self.run_Serial)
        # self.serialThread.start()
        sleep(0.1)

        self.start_qspy()
        self.qspy_polling_thread = threading.Thread(group=None, name="qthreadPolling", target=self.qspy_polling_loop)
        self.qspy_polling_thread.start()

        sleep(2)
        self.start_qutest()

        return self.run_Serial()
        # self.qutest_polling_thread.join()
        # self.qspy_polling_thread.join()



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
        self.isTesting = False
        click.echo(out)
        click.secho(err, fg="red")
        self.test_suite.on_finish()
        return

    # This function is executed in a seperate thread and continously forwards commands
    # from QSpy to the MCU via the udp channel.
    def qspy_polling_loop(self):

        click.secho("Starting polling of qspy...", fg='green')
        while self.qspy_process.poll() is None:
            try:
                data = self.qspy_socket.recv(1)
                click.secho("QSPY->MCU: {}".format(data), fg='yellow')
                self.send_to_MCU(data)
            except Exception as e:
                click.secho("Exception sending data to MCU: {}".format(e), fg='red')
        
        click.secho("Stopped polling QSpy. Return code: {}".format(self.qspy_process.returncode))
        outs, errs = self.qspy_process.communicate(timeout=15)
        click.secho(outs, fg='yellow')
        click.secho(errs, fg='red')

    # Processed data received from MCU via debug port.
    # Forwards the received data to the qspy-process.
    def on_testing_data_output(self, data):
        click.secho("D: {}".format(data), fg='yellow')
        self.qspy_socket.sendall(data)
        return super().on_testing_data_output(data)

    # Called when receiving a complete line from the MCU.
    # Since qspy-produces binary data we can ignore this.
    def on_testing_line_output(self, line):
        # click.secho("L: {}".format(line), fg='yellow')
        # self.qspy_socket.send(bytes(line, "utf-8"))
        return
    
    def teardown(self):
        click.echo("tearing down tests...")
        # self.qspy_socket.close()
        # self.qspy_process.terminate()
        return super().teardown()