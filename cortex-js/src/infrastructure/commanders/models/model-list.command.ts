import { CommandRunner, SubCommand, Option } from 'nest-commander';
import { ModelsCliUsecases } from '../usecases/models.cli.usecases';

interface ModelListOptions {
  format: 'table' | 'json';
}
@SubCommand({ name: 'list', description: 'List all models locally.' })
export class ModelListCommand extends CommandRunner {
  constructor(private readonly modelsCliUsecases: ModelsCliUsecases) {
    super();
  }

  async run(_input: string[], option: ModelListOptions): Promise<void> {
    const models = await this.modelsCliUsecases.listAllModels();
    option.format === 'table' ? console.table(models) : console.log(models);
  }

  @Option({
    flags: '-f, --format <format>',
    defaultValue: 'json',
    description: 'Print models list in table or json format',
  })
  parseModelId(value: string) {
    return value;
  }
}
