# Bendy Duration Lines
by [undefined0](user:13351341)

---

This mod simply allows duration lines coming out of triggers to bend to the final point, instead of always being straight. It also shows fade in and fade out durations on triggers that support it, as well as the color of any color or pulse triggers!

This mod also has performance settings! If you experience lag in the editor when lots of triggers are onscreen, you can turn down the limit of lines drawn (by default, GD uses 400 lines as a limit), or the precision of the lines. On the other hand, if you have a good computer, you can turn off culling, to see duration lines of triggers that are offscreen.

**Note**: This mod will <cr>not</c> fix any pre-existing issues that duration lines have (e.g. not showing up if placed after arrow triggers, going off into infinity, etc) - it just tries its best with RobTop's existing code to make the lines more accurate with arrow triggers. Duration lines will attempt to "snap" to arrow triggers if channels are not set up correctly - this is a vanilla GD bug!

TODO: remember to remove this but add the Y offset between the trigger and the first m_speedStart of the first trigger hit in LevelTools::posForTimeInternal to every point after the first trigger hit to transfer the y offset so every point doesnt just "snap" between arrow triggers
and remember to add it to the X instead if it's currently rotated
