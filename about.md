# Multiplayer Editor

Ever wanted to build a level with a friend in real time instead of sending the level back and forth? That's what this mod is for. It lets two or more people join the same editor session over the network and edit a level in real time, kind of like a collab doc but for Geometry Dash levels.

This mod was inspired by [Editor Collab](https://geode-sdk.org/mods/alk.editor-collab), which is a great mod and does this whole live collab thing really well. The one catch is that hosting a level there requires a one time purchase.
I wanted there to be a free option too, so people who just want to build a level with a friend without paying anything have a way to do that. This mod won't have every feature Editor Collab has, at least not right away, but the goal is to get it to a similar place over time while staying free and open source.

## THIS MOD IS EXPERIMENTAL!!

I want to be upfront about this: the mod is still in an early, experimental state. Editor sync is a genuinely messy thing to get right, and while most of the core stuff works, you will probably run into bugs, weird edge cases, or things not syncing quite well. Please make backups of your levels before using this in a real project (i will be implementing an auto backup thing).
I'm actively working on making it more stable and adding more sync coverage over time, so expect updates that fix things and expand what's supported.

If something breaks or doesn't sync properly, let me know, bug reports genuinely help this get better faster.

## How it connects

This mod uses peer to peer networking through [enet](https://github.com/lsalzman/enet).

One person hosts the session and others connect directly to them using their IP address.

Since this is direct P2P, using something like Hamachi or another virtual LAN tool is recommended if you're not on the same network, especially since NAT/port forwarding can be a pain otherwise.
