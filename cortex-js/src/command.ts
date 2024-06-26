#!/usr/bin/env node
import { CommandFactory } from 'nest-commander';
import { CommandModule } from './command.module';
import updateNotifier from 'update-notifier';
import packageJson from './../package.json';
import { TelemetryUsecases } from './usecases/telemetry/telemetry.usecases';
import { TelemetrySource } from './domain/telemetry/telemetry.interface';
import { AsyncLocalStorage } from 'async_hooks';
import { ContextService } from './util/context.service';

export const asyncLocalStorage = new AsyncLocalStorage();

async function bootstrap() {
  let telemetryUseCase: TelemetryUsecases | null = null;
  let contextService: ContextService | null = null;
  const app = await CommandFactory.createWithoutRunning(CommandModule, {
    logger: ['warn', 'error'],
    errorHandler: async (error) => {
      await telemetryUseCase!.createCrashReport(error, TelemetrySource.CLI);
      console.error(error);
      process.exit(1);
    },
    serviceErrorHandler: async (error) => {
      await telemetryUseCase!.createCrashReport(error, TelemetrySource.CLI);
      console.error(error);
      process.exit(1);
    },
  });

  telemetryUseCase = await app.resolve(TelemetryUsecases);
  contextService = await app.resolve(ContextService);

  const anonymousData = await telemetryUseCase!.updateAnonymousData();

  await contextService!.init(async () => {
    contextService!.set('source', TelemetrySource.CLI);
    contextService!.set('sessionId', anonymousData?.sessionId);
    telemetryUseCase!.sendActivationEvent(TelemetrySource.CLI);
    telemetryUseCase!.sendCrashReport();
    return CommandFactory.runApplication(app);
  });

  const notifier = updateNotifier({
    pkg: packageJson,
    updateCheckInterval: 1000 * 60 * 60, // 1 hour
    shouldNotifyInNpmScript: true,
  });
  notifier.notify({
    isGlobal: true,
  });
}

bootstrap();
