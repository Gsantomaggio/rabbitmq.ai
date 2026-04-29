package osiris

// Doc marks this package as holding Osiris chunk helpers.
const Doc = "see chunk.go for Deliver framing"

// Message entries inside an Osiris chunk follow the broker's EntryTypeAndSize layout.
// Full parsing requires the Osiris reference implementation; consumers can read raw
// `Deliver` payloads from `Client.Deliveries()` and parse offline.
