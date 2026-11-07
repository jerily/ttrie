package require tcltest 2
package require ttrie 0.1.0
namespace import ::tcltest::test

test search-1 {All distances and deterministic ties; duplicates are idempotent} -body {
    set trie [::ttrie::create names {tk tclx tcl tcl abc tcl-tk}]
    set result [$trie search tcl]
    $trie destroy
    set result
} -result {{name tcl distance 0} {name tclx distance 1} {name tk distance 2}}

test search-2 {Punctuation and case are preserved; exact-only and empty results} -body {
    set trie [::ttrie::create names {a-c a_c a.c a+c TCL tcl}]
    set result [list [$trie search a_c 0] [$trie search TCL 0] [$trie search unknown] [$trie contains a]]
    $trie destroy
    set result
} -result {{{name a_c distance 0}} {{name TCL distance 0}} {} 0}

test lifecycle-1 {Namespaces, renaming, insertion and interpreter cleanup} -body {
    set result [namespace eval sample {
        set trie [::ttrie::create index]
        $trie insert demo
        rename $trie renamed
        set result [renamed contains demo]
        renamed destroy
        set result
    }]
    namespace delete sample
    interp create child
    child eval [list set auto_path $auto_path]
    child eval {package require ttrie; ::ttrie::create names {a b c}}
    interp delete child
    set result
} -result 1

test validation-1 {Reject invalid inputs without replacing existing commands} -body {
    ::ttrie::create names {abc}
    set result {}
    foreach command [list {::ttrie::create names} {::ttrie::create bad {ok {}}} \
        [list names insert [string repeat a 256]] [list names insert "a\u0000b"] \
        {names search {}} {names search a -1} {names search a 3} {names search a nope} \
        {names search} {names insert a b} {names destroy extra}] {
        lappend result [catch $command]
    }
    lappend result [names contains abc] [expr {![llength [info commands bad]]}]
    names destroy
    lsort -unique $result
} -result 1

# Independent full-matrix reference checks the pruned trie on every short word.
proc words {prefix depth} {
    set result {}
    if {$prefix ne {}} {lappend result $prefix}
    if {$depth > 0} {
        foreach c {a b c} {lappend result {*}[words $prefix$c [expr {$depth - 1}]]}
    }
    return $result
}
proc distance {a b} {
    for {set i 0} {$i <= [string length $a]} {incr i} {set matrix($i,0) $i}
    for {set j 0} {$j <= [string length $b]} {incr j} {set matrix(0,$j) $j}
    for {set i 1} {$i <= [string length $a]} {incr i} {
        for {set j 1} {$j <= [string length $b]} {incr j} {
            set before_i [expr {$i - 1}]
            set before_j [expr {$j - 1}]
            set cost [expr {[string index $a $before_i] ne [string index $b $before_j]}]
            set matrix($i,$j) [expr {min($matrix($before_i,$j)+1, $matrix($i,$before_j)+1, $matrix($before_i,$before_j)+$cost)}]
        }
    }
    return $matrix([string length $a],[string length $b])
}
test reference-1 {Exhaustive insertions, deletions, substitutions and pruning} -body {
    set keys [lsort [words {} 3]]
    ::ttrie::create original [lreverse $keys]
    ::ttrie::load names [original serialize]
    original destroy
    foreach query [words {} 4] {
        foreach limit {0 1 2} {
            set expected {}
            foreach key $keys {
                set d [distance $key $query]
                if {$d <= $limit} {lappend expected [list name $key distance $d]}
            }
            set expected [lsort -integer -index 3 $expected]
            if {[names search $query $limit] ne $expected} {error "Mismatch: $query/$limit"}
        }
    }
    names destroy
    return ok
} -result ok


test snapshot-1 {Portable format has deterministic bytes} -body {
    ::ttrie::create original {a}
    set hex [binary encode hex [original serialize]]
    original destroy
    set hex
} -result 545452494500010000000001016101

test snapshot-2 {Binary file round trip preserves searches and survives producer destruction} -body {
    ::ttrie::create original {tcl tk tclx a.c café}
    set expected {}
    foreach query {tcl web encrytion café unknown} {lappend expected [original search $query]}
    set bytes [original serialize]
    original destroy
    set channel [open [file join $::tcltest::temporaryDirectory index.bin] wb]
    puts -nonewline $channel $bytes
    close $channel
    set channel [open [file join $::tcltest::temporaryDirectory index.bin] rb]
    set restored [::ttrie::load restored [read $channel]]
    close $channel
    file delete [file join $::tcltest::temporaryDirectory index.bin]
    set actual {}
    foreach query {tcl web encrytion café unknown} {lappend actual [$restored search $query]}
    set result [list [expr {$expected eq $actual}] [expr {$bytes eq [$restored serialize]}]]
    $restored destroy
    set result
} -result {1 1}

test snapshot-3 {Invalid data and command collisions do not publish or replace commands} -body {
    ::ttrie::create original {a}
    set bytes [original serialize]
    set rejected {}
    foreach invalid [list {} [string range $bytes 0 5] [string range $bytes 0 end-1] \
            [string replace $bytes 6 6 [binary format c 2]] $bytes\x00] {
        lappend rejected [catch {::ttrie::load bad $invalid}]
    }
    lappend rejected [catch {::ttrie::load original $bytes}] \
        [catch {original serialize extra}] [expr {[info commands bad] eq {}}]
    original destroy
    lsort -unique $rejected
} -result 1

test snapshot-4 {Empty indexes round trip} -body {
    ::ttrie::create original {}
    ::ttrie::load restored [original serialize]
    set result [restored search web]
    original destroy
    restored destroy
    set result
} -result {}

test snapshot-5 {Loaded tries support further insertion} -body {
    ::ttrie::create original {abc}
    ::ttrie::load restored [original serialize]
    restored insert abd
    set result [list [restored contains abc] [restored contains abd] [original contains abd]]
    original destroy
    restored destroy
    set result
} -result {1 1 0}

test snapshot-boundary {Maximum input length survives serialization} -body {
    set value [string repeat a 255]
    ::ttrie::create original [list $value]
    ::ttrie::load restored [original serialize]
    set result [restored contains $value]
    original destroy
    restored destroy
    set result
} -result 1

set failed $::tcltest::numTests(Failed)
::tcltest::cleanupTests
exit [expr {$failed != 0}]
