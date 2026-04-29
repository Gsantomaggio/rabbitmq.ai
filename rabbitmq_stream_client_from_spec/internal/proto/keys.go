package proto

// Client command keys (request: MSB = 0). Response keys are KeyResponse = Key | 0x8000.
const (
	KeyDeclarePublisher        uint16 = 0x0001
	KeyPublish                 uint16 = 0x0002
	KeyPublishConfirm          uint16 = 0x0003
	KeyPublishError            uint16 = 0x0004
	KeyQueryPublisherSequence  uint16 = 0x0005
	KeyDeletePublisher         uint16 = 0x0006
	KeySubscribe               uint16 = 0x0007
	KeyDeliver                 uint16 = 0x0008
	KeyCredit                  uint16 = 0x0009
	KeyStoreOffset             uint16 = 0x000a
	KeyQueryOffset             uint16 = 0x000b
	KeyUnsubscribe             uint16 = 0x000c
	KeyCreate                  uint16 = 0x000d
	KeyDelete                  uint16 = 0x000e
	KeyMetadata                uint16 = 0x000f
	KeyMetadataUpdate          uint16 = 0x0010
	KeyPeerProperties          uint16 = 0x0011
	KeySaslHandshake           uint16 = 0x0012
	KeySaslAuthenticate        uint16 = 0x0013
	KeyTune                    uint16 = 0x0014
	KeyOpen                    uint16 = 0x0015
	KeyClose                   uint16 = 0x0016
	KeyHeartbeat               uint16 = 0x0017
	KeyRoute                   uint16 = 0x0018
	KeyPartitions              uint16 = 0x0019
	KeyConsumerUpdate          uint16 = 0x001a
	KeyCommandVersionsExchange uint16 = 0x001b
	KeyStreamStats             uint16 = 0x001c
	KeyCreateSuperStream       uint16 = 0x001d
	KeyDeleteSuperStream       uint16 = 0x001e
)

// DefaultVersion is the wire protocol version used for outbound commands.
const DefaultVersion uint16 = 1

// ResponseKey converts a request key to its response key (sets MSB).
func ResponseKey(req uint16) uint16 { return req | 0x8000 }

// IsResponse reports whether k is a response frame (MSB set).
func IsResponse(k uint16) bool { return k&0x8000 != 0 }
