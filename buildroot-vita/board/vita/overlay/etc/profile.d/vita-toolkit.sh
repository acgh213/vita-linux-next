# Vita Linux Workbench shell integration.
#
# Sourced by /etc/profile for every shell.  It must stay silent, must never
# fail a login, and must never override a value the user already set.
# Nothing here mounts anything: the toolkit payload is activated explicitly
# with vita-toolkit-session.

if [ -d /opt/vita-toolkit ] && [ -r /opt/vita-toolkit/VERSION ]; then
    if [ -z "${VITA_TOOLKIT_ROOT:-}" ]; then
        VITA_TOOLKIT_ROOT=/opt/vita-toolkit
        export VITA_TOOLKIT_ROOT
    fi
    if [ -d /opt/vita-toolkit/bin ]; then
        case ":${PATH}:" in
            *:/opt/vita-toolkit/bin:*) ;;
            *) PATH="${PATH}:/opt/vita-toolkit/bin"; export PATH ;;
        esac
    fi
fi
