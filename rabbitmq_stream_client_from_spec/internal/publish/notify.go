// Package publish reserved for publish-side helpers (confirm/error fan-in).
package publish

import "github.com/gsantomaggio/rmqstream/internal/proto"

// PublishConfirm is re-exported for clarity when wiring handlers.
type PublishConfirm = proto.PublishConfirm
