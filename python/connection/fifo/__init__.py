"""FIFO (named pipe) connection module."""

from . import fifo, fifoserver, sync_fifoclient, sync_fifoserver

__all__ = ["fifo", "fifoserver", "sync_fifoclient", "sync_fifoserver"]
