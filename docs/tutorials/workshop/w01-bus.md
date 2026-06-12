# Workshop W01: The Bus

**Goal:** See how a Bus lets events flow without the sender knowing who's listening.

## The primitive

A `Bus` is a message board. Anyone can pin a message (publish), anyone can
listen for messages matching a topic (subscribe), and neither knows the other
exists. This is the pattern that lets Rage just *listen* for damage events
without the game loop even knowing Rage is installed.

## Run it

```bash
cmake -B build && cmake --build build -j
./build/examples/workshop/w01_bus/workshop_w01_bus
```

## What you should see

```
Publishing 5 damage...
  subscriber: +5 damage (total: 5)
Publishing 3 damage...
  subscriber: +3 damage (total: 8)
Publishing 7 damage...
  subscriber: +7 damage (total: 15)
Final total: 15
After unsubscribe, publishing 10 (should be ignored)...
Total still: 15 (unchanged)
```

## How it works

1. `TopicDef<int>{"damage_dealt"}` defines a typed topic
2. `.on(bus)` binds it to a bus, returning a `Topic<int>`
3. `.subscribe(handler)` registers a callback, returns a `SubscriptionId`
4. `.publish(value)` delivers to every active subscriber
5. `bus.unsubscribe(id)` removes the handler

## Key insight

The publisher and subscriber have no reference to each other. They connect
through the Bus. This is the decoupling that makes effects additive — snap on
a subscriber, snap it off, no other code changes.
