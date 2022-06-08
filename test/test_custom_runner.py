from platformio.public import TestRunnerBase
import click
import string
from subprocess import Popen, PIPE, CREATE_NEW_CONSOLE
import socket
import threading
import time

class CustomTestRunner(TestRunnerBase):

    def stage_uploading(self):

        # launch a QSPY process that is opened in a dedicated shell
        click.secho("Opening QSPY session...")
        self.qspy_process = Popen('qspy', creationflags=CREATE_NEW_CONSOLE)

        click.echo("TestRunner connecting to qspy on port 6601")
        self.qspy_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.qspy_socket.connect(('localhost', 6601))
        return super().stage_uploading()

    def qutest_polling_loop(self):

        # Poll qutest output while the process is still alive.
        while self.qutest_process.poll() is None:
            outputs = self.qutest_process.stdout.readlines()

            for line in outputs:
                click.echo(line)

            sleep(1)

        return

    def stage_testing(self):

        # Run qutest and connect to the qspy process on the default port.
        click.secho("Running qutest...")
        # qutest will write its output to PIPE, that we can read periodically.
        # However, since the reading is blocking, we will delegate that to another thread, from where the output
        # will be processed.
        self.qutest_process = Popen('qutest', creationflags=CREATE_NEW_CONSOLE, stdin=PIPE, stdout=PIPE, stderr=PIPE)

        # Start polling
        self.qutest_polling_thread = threading.Thread(group=None, name="qutestPolling", target=qutest_polling_loop, args=(self,), daemon=True)

        # wait for polling thread to end, which happens when qutest has stopped and all outputs have been processed.
        self.qutest_polling_thread.join()

        # TODO: Kill qspy-process and close console

        return super().stage_testing()

    # Processed data received from MCU via debug port.
    # Forwards the received data to the qspy-process.
    def on_testing_data_output(self, data):
        click.echo(data)
        self.qspy_socket.send(data)
        #return super().on_testing_line_output(line)

    # Called when receiving a complete line from the MCU.
    # Since qspy-produces binary data we can ignore this.
    def on_testing_line_output(self, line):
        return
    
    def teardown(self):
        self.qspy_socket.close()
        self.qspy_process.terminate()
        return super().teardown()