/* Taking SETUP.EXE's place, and giving it back.
 *
 * skidcfg ships as SKIDCFG.EXE and is happy to stay that way. This is the
 * other option: put it where the original was, so that anything already
 * telling people to run SETUP.EXE keeps being right, and so that a run of the
 * original cannot quietly rewrite a command line naming a driver it has never
 * heard of.
 *
 *     SKIDCFG /I    SETUP.EXE becomes SETUP.ORG, this takes the name
 *     SKIDCFG /U    the reverse, exactly
 *
 * The rule the whole file is built on is that /U has to put the directory
 * back the way it was found, so nothing here ever destroys anything.
 * The original is renamed, never overwritten and never deleted, and every
 * refusal below exists to keep it that way: no step runs unless the step that
 * undoes it will work.
 */
#ifndef INSTALL_H
#define INSTALL_H

/* Where things stand in the current directory. */
enum inst_state {
    INST_NONE,     /* a real SETUP.EXE, untouched */
    INST_DONE,     /* SETUP.EXE is skidcfg, SETUP.ORG is the original */
    INST_ABSENT,   /* no SETUP.EXE at all, so nothing to take over */
    INST_FOREIGN,  /* SETUP.ORG exists but SETUP.EXE is not skidcfg */
    INST_ORG_OURS, /* both are skidcfg, so the original is not here */
    INST_UNSURE,   /* SETUP.EXE is skidcfg and there is no SETUP.ORG */
    INST_UNREAD    /* a file is there and could not be read through */
};

enum inst_state inst_state(void);

/* Both return 0 on success. They print what they did or why they would not,
 * so a caller has nothing to add. self is argv[0]: DOS 3.0 and later put the
 * full path of the running program there, which is how this copies itself. */
int inst_install(const char *self);
int inst_uninstall(void);

#endif
