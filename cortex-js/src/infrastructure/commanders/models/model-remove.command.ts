import { SubCommand } from 'nest-commander';
import { ModelsCliUsecases } from '@commanders/usecases/models.cli.usecases';
import { exit } from 'node:process';
import { SetCommandContext } from '../decorators/CommandContext';
import { ContextService } from '@/infrastructure/services/context/context.service';
import { CortexUsecases } from '@/usecases/cortex/cortex.usecases';
import { BaseCommand } from '../base.command';

@SubCommand({
  name: 'remove',
  description: 'Remove a model by ID locally.',
  arguments: '<model_id>',
  argsDescription: {
    model_id: 'Model to remove',
  },
})
@SetCommandContext()
export class ModelRemoveCommand extends BaseCommand {
  constructor(
    private readonly modelsCliUsecases: ModelsCliUsecases,
    readonly contextService: ContextService,
    readonly cortexUseCases: CortexUsecases,
  ) {
    super(cortexUseCases);
  }

  async runCommand(passedParams: string[]): Promise<void> {
    if (passedParams.length === 0) {
      console.error('Model ID is required');
      exit(1);
    }

    await this.modelsCliUsecases.removeModel(passedParams[0]).then(console.log);
  }
}
