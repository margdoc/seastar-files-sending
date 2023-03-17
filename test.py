import argparse
import subprocess
from random import randbytes
import tempfile
import filecmp
import shutil
from time import sleep


def create_file(file_size: int, file_path: str):
    with open(file_path, "wb") as file:
        file.write(randbytes(file_size))

def run(client: str, server: str, port: int, size: int, n: int, verbose: bool):
    with tempfile.TemporaryDirectory() as source, \
         tempfile.TemporaryDirectory() as destination:
        server = subprocess.Popen(
            [server, "--port", str(port), "--dir", destination, "--smp", "1", "--logger-log-level", "server=debug"],
            stdout=None if verbose else subprocess.DEVNULL,
            stderr=subprocess.STDOUT)
        
        sleep(0.5)

        clients = []
        files = []

        for i in range(n):
            file = source + "/" + str(i)
            create_file(size, file)
            files.append(file)

        for file in files:
            clients.append(subprocess.Popen(
                [client, "--file", file, "--server", "127.0.0.1", "--smp", "1", "--port", str(port), "--logger-log-level", "client=debug"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.STDOUT))
        
        for client in clients:
            client.wait()

        server.terminate()
        server.wait()

        for i in range(n):
            source_file = source + "/" + str(i)
            destination_file = destination + "/" + str(i)

            try:
                if not filecmp.cmp(source_file, destination_file, shallow=False):
                    shutil.copyfile(source_file, "./source_" + str(i))
                    shutil.copyfile(destination_file, "./destination_" + str(i))
                    print(f"ERROR: File destination_{i} differs from source_{i}")
                    exit(1)
            except FileNotFoundError as err:
                if err.filename == destination_file:
                    print(f"ERROR: File with index {i} not found in the server's destination folder")
                    exit(1)
                else:
                    raise err

    print("Passed")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Process some integers.')
    parser.add_argument('client', type=str, help='client\'s executable')
    parser.add_argument('server', type=str, help='server\'s executable')

    parser.add_argument('-v', action='store_true', help='verbose', default=False)
    parser.add_argument('-s', type=int, help='size of the file to be send (s * 4096)', required=True)
    parser.add_argument('-n', type=int, help='number of files to be send', default=1)
    parser.add_argument('-p', type=int, help='port on which the server will listen', default=10000)
    
    args = parser.parse_args()

    run(args.client, args.server, args.p, args.s * 4096, args.n, args.v)