import { trim, DEFAULT_TRIM_OPTIONS } from './index';

// Expose a stable global for the KDE QJSEngine integration.
(globalThis as unknown as { TrimmehCore: { trim: typeof trim; DEFAULT_TRIM_OPTIONS: typeof DEFAULT_TRIM_OPTIONS } }).TrimmehCore = {
    trim,
    DEFAULT_TRIM_OPTIONS,
};
