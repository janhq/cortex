import { CortexUsecases } from '@/usecases/cortex/cortex.usecases';
import { ModelsUsecases } from '@/usecases/models/models.usecases';
import { CommandRunner, SubCommand, Option } from 'nest-commander';
import { exit } from 'node:process';
import { ChatCliUsecases } from '../usecases/chat.cli.usecases';
import { defaultCortexCppHost, defaultCortexCppPort } from 'constant';

type RunOptions = {
  model?: string;
};

@SubCommand({
  name: 'run',
  description: 'EXPERIMENTAL: Shortcut to start a model and chat',
})
export class RunCommand extends CommandRunner {
  constructor(
    private readonly modelsUsecases: ModelsUsecases,
    private readonly cortexUsecases: CortexUsecases,
    private readonly chatCliUsecases: ChatCliUsecases,
  ) {
    super();
  }

  async run(_input: string[], option: RunOptions): Promise<void> {
    const modelId = option.model;
    if (!modelId) {
      console.error('Model ID is required');
      exit(1);
    }

    await this.cortexUsecases.startCortex(
      defaultCortexCppHost,
      defaultCortexCppPort,
      false,
    );
    await this.modelsUsecases.startModel(modelId);
    await this.chatCliUsecases.chat(modelId);
  }

  @Option({
    flags: '-m, --model <model_id>',
    description: 'Model Id to start chat with',
  })
  parseModelId(value: string) {
    return value;
  }
}
