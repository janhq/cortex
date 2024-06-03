import { Module } from '@nestjs/common';
import { ModelsModule } from './usecases/models/models.module';
import { DatabaseModule } from './infrastructure/database/database.module';
import { ConfigModule } from '@nestjs/config';
import { CortexModule } from './usecases/cortex/cortex.module';
import { ServeCommand } from './infrastructure/commanders/serve.command';
import { ModelsCommand } from './infrastructure/commanders/models.command';
import { ExtensionModule } from './infrastructure/repositories/extensions/extension.module';
import { InitCommand } from './infrastructure/commanders/init.command';
import { HttpModule } from '@nestjs/axios';
import { InitRunModeQuestions } from './infrastructure/commanders/questions/init.questions';
import { ModelListCommand } from './infrastructure/commanders/models/model-list.command';
import { ModelPullCommand } from './infrastructure/commanders/models/model-pull.command';
import { CortexCommand } from './infrastructure/commanders/cortex-command.commander';
import { ChatCommand } from './infrastructure/commanders/chat.command';
import { ModelStartCommand } from './infrastructure/commanders/models/model-start.command';
import { ModelStopCommand } from './infrastructure/commanders/models/model-stop.command';
import { ModelGetCommand } from './infrastructure/commanders/models/model-get.command';
import { ModelRemoveCommand } from './infrastructure/commanders/models/model-remove.command';
import { RunCommand } from './infrastructure/commanders/shortcuts/run.command';
import { InitCudaQuestions } from './infrastructure/commanders/questions/cuda.questions';
import { ModelUpdateCommand } from './infrastructure/commanders/models/model-update.command';
import { AssistantsModule } from './usecases/assistants/assistants.module';
import { CliUsecasesModule } from './infrastructure/commanders/usecases/cli.usecases.module';
import { MessagesModule } from './usecases/messages/messages.module';
import { FileManagerModule } from './file-manager/file-manager.module';
import { PSCommand } from './infrastructure/commanders/ps.command';
import { KillCommand } from './infrastructure/commanders/kill.command';
import { PresetCommand } from './infrastructure/commanders/presets.command';

@Module({
  imports: [
    ConfigModule.forRoot({
      isGlobal: true,
      envFilePath:
        process.env.NODE_ENV !== 'production' ? '.env.development' : '.env',
    }),
    DatabaseModule,
    ModelsModule,
    CortexModule,
    ExtensionModule,
    HttpModule,
    CliUsecasesModule,
    AssistantsModule,
    MessagesModule,
    FileManagerModule,
  ],
  providers: [
    CortexCommand,
    ModelsCommand,
    ServeCommand,
    ChatCommand,
    InitCommand,
    PSCommand,
    KillCommand,
    PresetCommand,

    // Questions
    InitRunModeQuestions,
    InitCudaQuestions,

    // Model commands
    ModelStartCommand,
    ModelStopCommand,
    ModelListCommand,
    ModelGetCommand,
    ModelRemoveCommand,
    ModelPullCommand,
    ModelUpdateCommand,

    // Shortcuts
    RunCommand,
  ],
})
export class CommandModule {}
