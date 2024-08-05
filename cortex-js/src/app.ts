import { NestFactory } from '@nestjs/core';
import { DocumentBuilder, SwaggerModule } from '@nestjs/swagger';
import { AppModule } from './app.module';
import { FileManagerService } from './infrastructure/services/file-manager/file-manager.service';
import { ValidationPipe } from '@nestjs/common';
import { TelemetryUsecases } from './usecases/telemetry/telemetry.usecases';
export const getApp = async () => {
  const app = await NestFactory.create(AppModule, {
    snapshot: true,
    cors: true,
    logger: console,
  });

  // Set the global prefix for the API /v1/
  app.setGlobalPrefix('v1');

  const fileService = app.get(FileManagerService);
  await fileService.getConfig();

  const telemetryService = await app.resolve(TelemetryUsecases);
  await telemetryService.initInterval();

  app.useGlobalPipes(
    new ValidationPipe({
      transform: true,
      enableDebugMessages: true,
    }),
  );

  const config = new DocumentBuilder()
    .setTitle('Cortex API')
    .setDescription(
      'Cortex API provides a command-line interface (CLI) for seamless interaction with Large Language Models (LLMs). It is fully compatible with the [OpenAI API](https://platform.openai.com/docs/api-reference) and enables straightforward command execution and management of LLM interactions.',
    )
    .setVersion('1.0')
    .addTag(
      'Inference',
      'This endpoint initiates interaction with a Large Language Models (LLM).',
    )
    .addTag(
      'Assistants',
      'These endpoints manage the lifecycle of an Assistant within a conversation thread.',
    )
    .addTag(
      'Models',
      'These endpoints provide a list and descriptions of all available models within the Cortex framework.',
    )
    .addTag(
      'Messages',
      'These endpoints manage the retrieval and storage of conversation content, including responses from LLMs and other metadata related to chat interactions.',
    )
    .addTag(
      'Threads',
      'These endpoints handle the creation, retrieval, updating, and deletion of conversation threads.',
    )
    .addTag(
      'Embeddings',
      'Endpoint for creating and retrieving embedding vectors from text inputs using specified models.',
    )
    .addTag(
      'Engines',
      'Endpoints for managing the available engines within Cortex.',
    )
    .addTag(
      'System',
      'Endpoints for stopping the Cortex API server, checking its status, and fetching system events.',
    )
    .addServer('http://127.0.0.1:1337')
    .build();
  const document = SwaggerModule.createDocument(app, config);

  SwaggerModule.setup('api', app, document);
  return app;
};
