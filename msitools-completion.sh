# msiinfo bash completion

__msiinfo_all_commands ()
{
        local i IFS=" "$'\n'
        for i in $(msiinfo --help | grep "^  [^-]"  | cut -d' ' -f 3)
        do
                case $i in
                *) echo $i;;
                esac
        done
}
__msiinfo_commands=
__msiinfo_commands="$(__msiinfo_all_commands 2>/dev/null)"

_msiinfo ()
{
    COMPREPLY=()

    in_array()
    {
        local i
        for i in $2; do
            [[ $i = $1 ]] && return 0
        done
        return 1
    }

    local cur prev
    # _get_comp_words_by_ref is in bash-completion >= 1.2, which EL-5 lacks.
    if type _get_comp_words_by_ref &>/dev/null; then
        _get_comp_words_by_ref cur prev
    else
        cur="${COMP_WORDS[COMP_CWORD]}"
        prev="${COMP_WORDS[COMP_CWORD-1]}"
    fi

    # parse main options and get command

    local options="--help --version"
    local command=
    local command_first=
    local path=

    local i w
    for (( i = 0; i < ${#COMP_WORDS[*]} - 1; i++ )); do
        w="${COMP_WORDS[$i]}"
        # command
        if in_array "$w" "$__msiinfo_commands"; then
            command="$w"
            command_first=$((i+1))
            break
        fi
    done

    # complete base options

    if [[ -z $command ]]; then
        if [[ $cur == -* ]]; then
            COMPREPLY=( $(compgen -W "$options $options_value" -- "$cur") )
            return 0
        fi

        case "$prev" in
            *)
                COMPREPLY=( $(compgen -W "$__msiinfo_commands" -- "$cur") )
                ;;
        esac

        return 0
    fi

    local after= after_more=
    case $command in
        streams|tables|extract|export|suminfo)
            after="msi"
            ;;
    esac

    local all_options="--help $options"
    local all_options_value=""

    # count non-option parameters

    local i w
    local last_option=
    local after_counter=0
    local after_word=
    for (( i = $command_first; i < ${#COMP_WORDS[*]} - 1; i++)); do
        w="${COMP_WORDS[$i]}"
        if [[ ${w:0:1} = - ]]; then
            if in_array "$w" "$all_options"; then
                last_option="$w"
                continue
            elif in_array "$w" "$all_options_value"; then
                last_option="$w"
                ((i++))
                continue
            fi
        else
            last_word="$w"
        fi
        in_array "$last_option" "$options_arches" || ((after_counter++))
    done

    local after_options=
    if [[ $after_counter -eq 0 ]] || [[ $after_more = true ]]; then
        case $after in
                msi)    _filedir msi;;
        esac
    fi
    if [[ $after_counter -eq 1 ]]; then
        case $command in
            extract)
                after_options="$(msiinfo streams "$last_word")" ;;
            export)
                after_options="$(msiinfo tables "$last_word")" ;;
        esac
    fi

    if [[ $cur != -* ]]; then
        all_options=
        all_options_value=
    fi

    COMPREPLY+=( $(compgen -W "$all_options $all_options_value $after_options" -- "$cur" ) )

    return 0
}

_msibuild ()
{
        local cur="${COMP_WORDS[COMP_CWORD]}"
        case "$cur" in
        -*)
                COMPREPLY=( $(compgen -W "-s -q -i -a" -- "$cur" ) )
                return 0
                ;;
        esac
}

complete -o default -o nospace -F _msiinfo msiinfo
complete -o default -o nospace -F _msibuild msibuild
