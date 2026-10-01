# Smallest pure GDScript module that installs without C++.
extends RefCounted


# Return a greeting for the given name.
static func message(name = "gd"):
	return "hello, %s" % name
