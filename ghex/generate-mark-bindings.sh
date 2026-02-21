#!/bin/bash

# Used to generate mark keybindings in ghex-application-window.c

for i in 0 1 2 3 4 5 6 7 8 9; do
	cat << EOF
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_${i},
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			${i});
	gtk_widget_class_add_binding_action (widget_class,
			GDK_KEY_KP_${i},
			GDK_CONTROL_MASK,
			"ghex.activate-mark",
			"i",
			${i});
EOF
done
