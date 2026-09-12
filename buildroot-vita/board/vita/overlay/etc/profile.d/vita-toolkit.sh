# Vita Linux Workbench shell integration.
#
# Sourced by /etc/profile for every shell. Must stay silent, must never fail a
# login, and must never override a value the user already set.

# First-party tools shipped in the initramfs.
case ":${PATH}:" in
    *:/usr/local/bin:*) ;;
    *) PATH="${PATH}:/usr/local/bin"; export PATH ;;
esac

# Toolkit payload, when S06toolkit has mounted it from the game card.
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
