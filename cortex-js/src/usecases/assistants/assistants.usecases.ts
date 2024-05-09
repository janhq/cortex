import { Inject, Injectable } from '@nestjs/common';
import { AssistantEntity } from '@/infrastructure/entities/assistant.entity';
import { Repository } from 'typeorm';
import { CreateAssistantDto } from '@/infrastructure/dtos/assistants/create-assistant.dto';
import { Assistant } from '@/domain/models/assistant.interface';

@Injectable()
export class AssistantsUsecases {
  constructor(
    @Inject('ASSISTANT_REPOSITORY')
    private assistantRepository: Repository<AssistantEntity>,
  ) {}

  create(createAssistantDto: CreateAssistantDto) {
    const assistant: Assistant = {
      ...createAssistantDto,
      object: 'assistant',
      created_at: Date.now(),
    };
    this.assistantRepository.insert(assistant);
  }

  async findAll(): Promise<Assistant[]> {
    return this.assistantRepository.find();
  }

  async findOne(id: string) {
    return this.assistantRepository.findOne({
      where: { id },
    });
  }

  async remove(id: string) {
    return this.assistantRepository.delete(id);
  }
}
