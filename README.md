# Hackable Lock

The concept behind this is to create a secure access controller that is also a bit hackable and enables end-user administrators to extend the software to add their own events or functionality.

## Version 0
The prototype hardware concept is an ESP32-type Bluetooth LE controller that acts as a Bluetooth LE "Peripheral"/"Server" role.

At runtime, the smart lock with be on a resource-constrained ESP32, and the client will be an Android, iOS, or nRF52 device.

### Crypto Protocol
The Hackable Lock should authenticate users that will initiate an action expressing an intent for a lock or door to open up.

At the time of requesting an unlock, there should be a mutual authentication of both parties.

There should be a central directory, named with a unique DNS label-like name (e.g. example.org).
The central directory should use a versioned public and private key used to identify itself.
Within the directory, Users should be uniquely identified by a byte-serializable string.

Users' clients should authenticate the private key of the central directory out-of-band and install it into a list of trust anchors in their client.
Over time, as it may change, clients should also support a message that installs an updated trust anchor for a central directory, using a later-versioned public key binding, signed by the previous key version.

The locks should be able to generate a private key and register its corresponding public key in a directory of locks.
Authentication of lock public keys should be performed out-of-band by a system administrator.
Once authenticated, a certificate should be created by the central directory and signed with its private key. 

The users should be able to generate a private key that is locally stored and never transmitted. Users should be able to publish an identity (liked by the unique byte-string that identifies the user and the public key) into the central directory, from which the smart lock will be able to reference when making authentication decisions.
