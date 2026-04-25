export const SESSION_STATUS = {
  ACTIVE: "active",
  ENDED: "ended",
  TIMED_OUT: "timed_out",
} as const;

export type SessionStatus = (typeof SESSION_STATUS)[keyof typeof SESSION_STATUS];

export const SAVE_RESPONSE_STATUS = {
  ENDED: SESSION_STATUS.ENDED,
  SAVED: "saved",
  STALE_IGNORED: "stale_ignored",
} as const;

export const PERSISTENCE_ERROR = {
  AUTH_INVALID: "auth_invalid",
  AUTH_REQUIRED: "auth_required",
  CHARACTER_OWNER_MISMATCH: "character_owner_mismatch",
  SESSION_EXPIRED: "session_expired",
  SESSION_ID_REQUIRED: "session_id_required",
  STALE_OR_INVALID_SESSION: "stale_or_invalid_session",
} as const;
