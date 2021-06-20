# High frequency trading risk server+client

C++17 HFT risk calculation system.
No third-party dependencies needed.

This is just a weekend hacking project.
It is probably useless in actual trading.

## Implemented features

* UNIX TCP server and client.
* Efficient storage of orders and instruments in hash tables (`std::unordered_map`).
* Rather than computing three net sums over all existing orders each time the net position is requested, the sums are updated into `InstrumentState` for each instrument each time the state of the server changes.
* Message serialization.
* Risk server capable of handling messages over TCP.
* Risk client capable of sending messages to the risk server over TCP.

## Missing features

* Message protocol `struct`s are not packed. It was easier to test the system this way. Encoding and decoding of packed `struct`s can be implemented later as an optimization to minimize network bandwidth.

## Example output

Please see `server.log` and `client.log` for example program output.

## Running the example

### Building

```
mkdir build
cd build
cmake ..
make -j8
```

### Running

Start the risk server, serving at `127.0.0.1:7001` with max buy position 20 and max sell position 15:
```
./bin/risk-server 127.0.0.1 7001 20 15
```
In a second terminal, send some messages with the client to the server at `127.0.0.1:7001` by running:
```
./bin/test 127.0.0.1 7001
```
Verify that your output matches `client.log` for the client and `server.log` for the server.

### Tested systems

#### Linux
* Debian 10 (buster)
* CMake 3.13.4
* clang 7.0.1-8.

#### macOS
* Apple M1, Big Sur 11.4
* CMake 3.20.3
* clang 12.0.5
