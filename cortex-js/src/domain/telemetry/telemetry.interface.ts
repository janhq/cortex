// TODO. move openTelemetry interfaces to dto
export interface TelemetryResource {
  osName: string;
  osVersion: string;
  architecture: string;
  appVersion: string;
  'service.name'?: string;
  source?: TelemetrySource;
  cpu?: string;
  gpus?: string;
  // todo: consider about sessionId
  // sessionId: string;
}

export interface CrashReportEvent {
  modelId?: string;
  sessionId: string;
  stack?: string;
}

export interface Resource {
  attributes: Attribute[];
}

export enum TelemetrySource {
  CLI = 'cli',
  CORTEX_SERVER = 'cortex-server',
  CORTEX_CPP = 'cortex-cpp',
}

export interface TelemetryEvent {
  resource: Resource;
  scopeLogs: ScopeLog[];
}

interface StringValue {
  stringValue: string;
}

interface ObjectValue {
  kvlist_value: {
    values: {
      key: string;
      value: StringValue;
    }[];
  };
}

export interface Attribute {
  key: string;
  value: StringValue | ObjectValue;
}

interface ScopeLog {
  scope: object;
  logRecords: TelemetryLog[];
}

export interface TelemetryLog {
  traceId: string;
  startTimeUnixNano?: string;
  endTimeUnixNano?: string;
  severityText: string;
  body: {
    stringValue: string;
  };
  attributes: Attribute[];
}

export interface CrashReportPayload {
  modelId?: string;
  endpoint?: string;
  command?: string;
}

export interface CrashReportAttributes {
  stack?: string;
  message: string;
  payload: CrashReportPayload;
}

export interface TelemetryEventMetadata {
  createdAt: string;
  sentAt: string | null;
}

export interface Telemetry {
  metadata: TelemetryEventMetadata;
  event: {
    resourceLogs: TelemetryEvent[];
  };
}
