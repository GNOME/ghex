#!/bin/sh

TEST_PROG="./main"

# GTK_DEBUG=interactive \
	G_ENABLE_DIAGNOSTIC=1 \
	G_MESSAGES_DEBUG=all \
	$TEST_PROG
