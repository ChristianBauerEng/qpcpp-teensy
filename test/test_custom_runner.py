from platformio.public import TestRunnerBase
import click
import string
from subprocess import Popen, PIPE, CREATE_NEW_CONSOLE
import socket

class CustomTestRunner(TestRunnerBase):

    def stage_uploading(self):

        # launch a QSPY process that is opened in a dedicated shell
        click.secho("Opening QSPY session...")
        self.qspy_process = Popen('qspy', creationflags=CREATE_NEW_CONSOLE)

        self.qspy_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.qspy_socket.connect(('localhost', 6601))

        return super().stage_uploading()


    def stage_testing(self):
        return super().stage_testing()

    def on_testing_data_output(self, data):
        click.echo(data)
        self.qspy_socket.send(data)
        #return super().on_testing_line_output(line)

    def on_testing_line_output(self, line):
        return
    
    def teardown(self):
        self.qspy_socket.close()
        self.qspy_process.terminate()
        return super().teardown()