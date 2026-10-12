"""Hard network kill switch.

The anonymizer never needs the network. Calling ``enforce()`` at start-up replaces
the socket layer of this Python process, so any attempt to open a connection
(by this program or by any library it loads) raises instead of sending data.
"""

import socket

_MESSAGE = "Network access is disabled in PDF Anonymizer (offline mode)."


class _BlockedSocket(socket.socket):
    def __init__(self, *args, **kwargs):
        raise OSError(_MESSAGE)


def _blocked(*args, **kwargs):
    raise OSError(_MESSAGE)


def enforce() -> None:
    socket.socket = _BlockedSocket
    socket.create_connection = _blocked
    socket.getaddrinfo = _blocked
    socket.gethostbyname = _blocked
    socket.gethostbyname_ex = _blocked
    socket.socketpair = _blocked
    socket.fromfd = _blocked


def is_enforced() -> bool:
    return socket.socket is _BlockedSocket
