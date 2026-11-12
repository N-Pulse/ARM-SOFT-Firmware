"""Compatibility launcher for the current decoding-link CLI.
No port opens on import; pyserial is loaded only by main().
"""
from decoding_link import main
if __name__=='__main__': main()
