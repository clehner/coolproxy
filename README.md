# coolproxy

Web proxy server. Implemented using a single-threaded epoll-based event loop.
Handles concurrent connections. Has some work towards handling persistent
connections on the upstream side, but not yet on the client side.

## Usage

	make
	make test
	./coolproxy <port>

## Design choices

The program is structured into modules which are like classes. In each
module we try to follow a consistent object-oriented style where the first
argument of a function is a struct of the module's type, and the names of
functions begin with the module's name. The modules are decoupled when
possible, for simplicity and reusability, using callbacks
(continuation-passing style). They store state in their object/struct
instead of using global variables.

The modules are for server, client, worker, HTTP parser, and event loop.

The event loop module multiplexes IO, and passes control the other
modules through callbacks when their sockets receive data. It runs once and
stays running throughout the program's execution.

The HTTP parser module handles HTTP requests and responses, headers, and
data, on both the client and worker side.

The client module handles an HTTP client that connects to the proxy server.

The server module accepts incoming connections and creates client objects to
handle them.

The worker module makes outgoing requests on behalf of the clients, and
handles data received from the upstream servers.

## Event-driven Architecture

Using an event-driven approach allows the server to potentially scale to
serve many concurrent requests, as in the [The C10K problem][1], by using
nonblocking IO and not taking on the overhead of threads is not used. In a
more sophisticated implementation, there may be multiple worker threads to
take advantage of multiple processors, CPU pipelining, etc.

Currently the address resolution is implemented in a blocking way, but this
could be changed by using the asynchronous getaddrinfo_a(3) or a standalone
nonblocking DNS library such as [c-ares][2].

The event loop uses epoll(7), which is a feature of the Linux kernel. The
implementation in this program could be extended to support other event
loops such as kqueue on BSD/OSX, or select/poll for even more support. Or it
could be changed to use a library such as libuv or libevent to handle the
multiple event loop backends.

## Testing

The test script makes two curl(1) requests, one through the proxy and one
direct. It compares the output to detect success.

## Code style

We try to use a consistent code style, and have a style check file to
enforce it. This could be extended to make the code style more consistent.

## Issues and limitations

There are sometimes segfaults. Interaction between the states of the proxy
client, upstream connection (worker) client, and server is not completely
solid.

Portability could be improved by using other event loop implementations, as
discussed above.

In addition as the current integration test using curl, a testing suite
could be implemented to test the HTTP parser and other modules, and how the
server handles persistent connections on the downstream or upstream side, 

HTTPS support is not yet implemented. Support for HTTPS could be added e.g.
by linking with openssl.

More descriptive error messages could be added.

Support for handling HTTP payloads from the client, i.e. with POST or PUT
requests, is not yet tested.

## License

Copyright &copy; 2014 Charles Lehner

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

[1]: http://www.kegel.com/c10k.html
[2]: http://c-ares.haxx.se/
