# Bendy Duration Lines
by [undefined0](user:13351341)

---

This mod allows duration lines coming out of triggers to bend to the final point if any arrow triggers mean they should, instead of always being straight.

This mod also comes with extra improvements to duration lines, such as:
- Showing fade in and fade out durations on certain triggers
- The color of any color or pulse triggers
- The variation on spawn triggers
- The strength and interval on shake triggers (suggested by [champik665](user:26344243))

This mod also has performance settings! If you experience lag in the editor when lots of triggers are onscreen, you can turn down the limit of lines drawn (by default, GD uses 400 lines as a limit), or the precision of the lines. On the other hand, if you have a good computer, you can turn off culling, to see duration lines of triggers that are offscreen.

**Note**: This mod will <cr>not</c> fix any pre-existing issues that duration lines have - it just tries its best with RobTop's existing code to make the lines more accurate with arrow triggers, but it will attempt to fix triggers placed after arrow triggers! Most notable issues come with triggers which are placed within half a block before an arrow trigger, as well as shake triggers placed during rotated gameplay.
